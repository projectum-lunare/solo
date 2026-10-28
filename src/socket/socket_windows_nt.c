/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <solo/platform.h>

#if SOLO_IS_WINDOWS_NT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>

#if (defined(_MSC_VER) && _MSC_VER <= 1200) || (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0501)
#define SOLO_USE_WINSOCK1 1
#include <winsock.h>
#else
#define SOLO_USE_WINSOCK1 0
#include <winsock2.h>
#include <ws2tcpip.h>

static const char *solo_gai_strerror(int err)
{
    (void)err;
    return "getaddrinfo failed";
}
#undef gai_strerror
#define gai_strerror solo_gai_strerror
#endif

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

struct solo_socket {
    SOCKET fd;
    solo_socket_type_t type;
    solo_socket_mode_t mode;
    int timeout_ms;
};

static int wsa_init_count = 0;

static void ensure_wsa_init(void)
{
    if (wsa_init_count == 0) {
        WSADATA wsa;
#if SOLO_USE_WINSOCK1
        WSAStartup(MAKEWORD(1, 1), &wsa);
#else
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }
    wsa_init_count++;
}

static void ensure_wsa_cleanup(void)
{
    if (wsa_init_count > 0) {
        wsa_init_count--;
        if (wsa_init_count == 0) {
            WSACleanup();
        }
    }
}

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char **err_msg)
{
    int socktype = (type == SOLO_SOCK_UDP) ? SOCK_DGRAM : SOCK_STREAM;
    SOCKET fd;
    u_long nonblocking;
    solo_socket_t *sock;

    ensure_wsa_init();

    fd = socket(AF_INET, socktype, 0);
    if (fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to create socket");
        }
        ensure_wsa_cleanup();
        return NULL;
    }

    if (mode == SOLO_SOCK_NONBLOCKING) {
        nonblocking = 1;
        ioctlsocket(fd, FIONBIO, &nonblocking);
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        closesocket(fd);
        ensure_wsa_cleanup();
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }

    sock->fd = fd;
    sock->type = type;
    sock->mode = mode;
    sock->timeout_ms = 0;
    return sock;
}

solo_socket_t *solo_socket_tcp(solo_socket_mode_t mode, char **err_msg)
{
    return solo_socket_create(SOLO_SOCK_TCP, mode, err_msg);
}

solo_socket_t *solo_socket_udp(solo_socket_mode_t mode, char **err_msg)
{
    return solo_socket_create(SOLO_SOCK_UDP, mode, err_msg);
}

void solo_socket_close(solo_socket_t *sock)
{
    if (sock == NULL)
        return;
    if (sock->fd != INVALID_SOCKET) {
        closesocket(sock->fd);
        sock->fd = INVALID_SOCKET;
        ensure_wsa_cleanup();
    }
    free(sock);
}

int solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char **err_msg)
{
    int opt = 1;
#if SOLO_USE_WINSOCK1
    struct sockaddr_in sin;
#else
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char port_str[16];
    int status;
#endif

    if (sock == NULL || sock->fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

#if SOLO_USE_WINSOCK1
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    if (host == NULL || strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0 || host[0] == '\0') {
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        sin.sin_addr.s_addr = inet_addr(host);
    }

    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    if (bind(sock->fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to bind socket");
        }
        return SOLO_SOCK_ERROR;
    }
#else
    sprintf(port_str, "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = (sock->type == SOLO_SOCK_UDP) ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (host != NULL &&
        (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0 || host[0] == '\0')) {
        host = NULL;
    }

    status = getaddrinfo(host, port_str, &hints, &res);
    if (status != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(gai_strerror(status));
        }
        return SOLO_SOCK_ERROR;
    }

    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    if (bind(sock->fd, res->ai_addr, (int)res->ai_addrlen) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to bind socket");
        }
        freeaddrinfo(res);
        return SOLO_SOCK_ERROR;
    }

    freeaddrinfo(res);
#endif

    if (sock->type == SOLO_SOCK_TCP) {
        if (listen(sock->fd, backlog > 0 ? backlog : 128) != 0) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("failed to listen on socket");
            }
            return SOLO_SOCK_ERROR;
        }
    }

    return SOLO_SOCK_OK;
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, solo_socket_mode_t mode, char **err_msg)
{
    solo_socket_t *sock = solo_socket_tcp(mode, err_msg);
    if (sock == NULL) {
        return NULL;
    }

    if (solo_socket_bind_sock(sock, host, port, backlog, err_msg) != SOLO_SOCK_OK) {
        solo_socket_close(sock);
        return NULL;
    }

    return sock;
}

solo_socket_t *solo_socket_udp_bind(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    solo_socket_t *sock = solo_socket_udp(mode, err_msg);
    if (sock == NULL) {
        return NULL;
    }

    if (solo_socket_bind_sock(sock, host, port, 0, err_msg) != SOLO_SOCK_OK) {
        solo_socket_close(sock);
        return NULL;
    }

    return sock;
}

int solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char **err_msg)
{
#if SOLO_USE_WINSOCK1
    struct sockaddr_in sin;
    unsigned long ip;
#else
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *rp = NULL;
    char port_str[16];
    int status;
    int connected = 0;
#endif

    if (sock == NULL || sock->fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

#if SOLO_USE_WINSOCK1
    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    ip = inet_addr(host);
    if (ip == (unsigned long)INADDR_NONE && strcmp(host, "255.255.255.255") != 0) {
        struct hostent *he = gethostbyname(host);
        if (he != NULL && he->h_addr_list != NULL && he->h_addr_list[0] != NULL) {
            memcpy(&ip, he->h_addr_list[0], sizeof(ip));
        } else {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("cannot resolve hostname");
            }
            return SOLO_SOCK_ERROR;
        }
    }
    sin.sin_addr.s_addr = ip;

    if (connect(sock->fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to connect");
        }
        return SOLO_SOCK_ERROR;
    }
    return SOLO_SOCK_OK;
#else
    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    sprintf(port_str, "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = (sock->type == SOLO_SOCK_UDP) ? SOCK_DGRAM : SOCK_STREAM;

    status = getaddrinfo(host, port_str, &hints, &res);
    if (status != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(gai_strerror(status));
        }
        return SOLO_SOCK_ERROR;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        if (connect(sock->fd, rp->ai_addr, (int)rp->ai_addrlen) == 0) {
            connected = 1;
            break;
        }
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            freeaddrinfo(res);
            return SOLO_SOCK_AGAIN;
        }
    }

    freeaddrinfo(res);

    if (!connected) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to connect");
        }
        return SOLO_SOCK_ERROR;
    }
    return SOLO_SOCK_OK;
#endif
}

solo_socket_t *solo_socket_connect(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    int res;
    solo_socket_t *sock = solo_socket_tcp(mode, err_msg);
    if (sock == NULL) {
        return NULL;
    }

    res = solo_socket_connect_sock(sock, host, port, err_msg);
    if (res != SOLO_SOCK_OK && res != SOLO_SOCK_AGAIN) {
        solo_socket_close(sock);
        return NULL;
    }

    return sock;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                  int *client_port, char **err_msg)
{
    SOCKET client_fd;
    solo_socket_t *client_sock;
#if SOLO_USE_WINSOCK1
    struct sockaddr_in sin;
    int addr_len = sizeof(sin);
#else
    struct sockaddr_storage addr;
    int addr_len = sizeof(addr);
    char host_buf[NI_MAXHOST];
    char serv_buf[NI_MAXSERV];
#endif

    if (server == NULL || server->fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

#if SOLO_USE_WINSOCK1
    client_fd = accept(server->fd, (struct sockaddr *)&sin, &addr_len);
    if (client_fd == INVALID_SOCKET) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            if (err_msg != NULL) {
                *err_msg = NULL;
            }
            return NULL;
        }
        if (err_msg != NULL) {
            *err_msg = safe_strdup("accept failed");
        }
        return NULL;
    }

    if (client_ip != NULL && ip_len > 0) {
        const char *ip_str = inet_ntoa(sin.sin_addr);
        if (ip_str != NULL) {
            strncpy(client_ip, ip_str, ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        } else {
            strncpy(client_ip, "unknown", ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        }
    }
    if (client_port != NULL) {
        *client_port = (int)ntohs(sin.sin_port);
    }
#else
    client_fd = accept(server->fd, (struct sockaddr *)&addr, &addr_len);
    if (client_fd == INVALID_SOCKET) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            if (err_msg != NULL) {
                *err_msg = NULL;
            }
            return NULL;
        }
        if (err_msg != NULL) {
            *err_msg = safe_strdup("accept failed");
        }
        return NULL;
    }

    if (client_ip != NULL && ip_len > 0) {
        if (getnameinfo((struct sockaddr *)&addr, addr_len, host_buf, sizeof(host_buf), serv_buf,
                        sizeof(serv_buf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
            strncpy(client_ip, host_buf, ip_len - 1);
            client_ip[ip_len - 1] = '\0';
            if (client_port != NULL) {
                *client_port = atoi(serv_buf);
            }
        } else {
            strncpy(client_ip, "unknown", ip_len - 1);
            client_ip[ip_len - 1] = '\0';
            if (client_port != NULL) {
                *client_port = 0;
            }
        }
    }
#endif

    client_sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (client_sock == NULL) {
        closesocket(client_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }
    client_sock->fd = client_fd;
    client_sock->type = SOLO_SOCK_TCP;
    client_sock->mode = server->mode;
    client_sock->timeout_ms = 0;

    if (server->mode == SOLO_SOCK_NONBLOCKING) {
        u_long nonblocking = 1;
        ioctlsocket(client_fd, FIONBIO, &nonblocking);
    }

    ensure_wsa_init();
    return client_sock;
}

int solo_socket_send(solo_socket_t *sock, const char *buf, size_t len)
{
    int total_sent = 0;
    int ret;

    if (sock == NULL || sock->fd == INVALID_SOCKET || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

    if (sock->mode == SOLO_SOCK_NONBLOCKING) {
        ret = send(sock->fd, buf, (int)len, 0);
        if (ret == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                return SOLO_SOCK_AGAIN;
            }
            return SOLO_SOCK_ERROR;
        }
        return ret;
    }

    while ((size_t)total_sent < len) {
        ret = send(sock->fd, buf + total_sent, (int)(len - (size_t)total_sent), 0);
        if (ret == SOCKET_ERROR) {
            return SOLO_SOCK_ERROR;
        }
        if (ret == 0) {
            break;
        }
        total_sent += ret;
    }

    return total_sent;
}

int solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len)
{
    int ret;
    if (sock == NULL || sock->fd == INVALID_SOCKET || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    ret = recv(sock->fd, buf, (int)max_len, 0);
    if (ret == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    return ret;
}

int solo_socket_write(solo_socket_t *sock, const char *buf, size_t len)
{
    return solo_socket_send(sock, buf, len);
}

int solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len)
{
    return solo_socket_recv(sock, buf, max_len);
}

int solo_socket_sendto(solo_socket_t *sock, const char *buf, size_t len,
                       const char *host, int port)
{
    int ret;
#if SOLO_USE_WINSOCK1
    struct sockaddr_in sin;
    unsigned long ip;
#else
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char port_str[16];
    int status;
#endif

    if (sock == NULL || sock->fd == INVALID_SOCKET || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

#if SOLO_USE_WINSOCK1
    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    ip = inet_addr(host);
    if (ip == (unsigned long)INADDR_NONE && strcmp(host, "255.255.255.255") != 0) {
        struct hostent *he = gethostbyname(host);
        if (he != NULL && he->h_addr_list != NULL && he->h_addr_list[0] != NULL) {
            memcpy(&ip, he->h_addr_list[0], sizeof(ip));
        } else {
            return SOLO_SOCK_ERROR;
        }
    }
    sin.sin_addr.s_addr = ip;

    ret = sendto(sock->fd, buf, (int)len, 0, (struct sockaddr *)&sin, sizeof(sin));
#else
    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    sprintf(port_str, "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    status = getaddrinfo(host, port_str, &hints, &res);
    if (status != 0) {
        return SOLO_SOCK_ERROR;
    }

    ret = sendto(sock->fd, buf, (int)len, 0, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
#endif

    if (ret == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    return ret;
}

int solo_socket_recvfrom(solo_socket_t *sock, char *buf, size_t max_len,
                         char *src_ip, size_t ip_len, int *src_port)
{
    int ret;
#if SOLO_USE_WINSOCK1
    struct sockaddr_in sin;
    int addr_len = sizeof(sin);
#else
    struct sockaddr_storage addr;
    int addr_len = sizeof(addr);
    char host_buf[NI_MAXHOST];
    char serv_buf[NI_MAXSERV];
#endif

    if (sock == NULL || sock->fd == INVALID_SOCKET || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

#if SOLO_USE_WINSOCK1
    ret = recvfrom(sock->fd, buf, (int)max_len, 0, (struct sockaddr *)&sin, &addr_len);
    if (ret == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }

    if (src_ip != NULL && ip_len > 0) {
        const char *ip_str = inet_ntoa(sin.sin_addr);
        if (ip_str != NULL) {
            strncpy(src_ip, ip_str, ip_len - 1);
            src_ip[ip_len - 1] = '\0';
        } else {
            strncpy(src_ip, "unknown", ip_len - 1);
            src_ip[ip_len - 1] = '\0';
        }
    }
    if (src_port != NULL) {
        *src_port = (int)ntohs(sin.sin_port);
    }
#else
    ret = recvfrom(sock->fd, buf, (int)max_len, 0, (struct sockaddr *)&addr, &addr_len);
    if (ret == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }

    if (src_ip != NULL && ip_len > 0) {
        if (getnameinfo((struct sockaddr *)&addr, addr_len, host_buf, sizeof(host_buf), serv_buf,
                        sizeof(serv_buf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
            strncpy(src_ip, host_buf, ip_len - 1);
            src_ip[ip_len - 1] = '\0';
            if (src_port != NULL) {
                *src_port = atoi(serv_buf);
            }
        } else {
            strncpy(src_ip, "unknown", ip_len - 1);
            src_ip[ip_len - 1] = '\0';
            if (src_port != NULL) {
                *src_port = 0;
            }
        }
    }
#endif

    return ret;
}

int solo_socket_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    u_long nonblocking;
    if (sock == NULL || sock->fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }
    nonblocking = (mode == SOLO_SOCK_NONBLOCKING) ? 1 : 0;
    if (ioctlsocket(sock->fd, FIONBIO, &nonblocking) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("ioctlsocket FIONBIO failed");
        }
        return SOLO_SOCK_ERROR;
    }
    sock->mode = mode;
    return SOLO_SOCK_OK;
}

solo_socket_mode_t solo_socket_get_mode(const solo_socket_t *sock)
{
    return sock ? sock->mode : SOLO_SOCK_BLOCKING;
}

solo_socket_type_t solo_socket_get_type(const solo_socket_t *sock)
{
    return sock ? sock->type : SOLO_SOCK_TCP;
}

int solo_socket_set_timeout(solo_socket_t *sock, int timeout_ms, char **err_msg)
{
    int tv = timeout_ms;
    if (sock == NULL || sock->fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }
    if (timeout_ms < 0) {
        tv = 0;
    }
    if (setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("setsockopt SO_RCVTIMEO failed");
        }
        return SOLO_SOCK_ERROR;
    }
    if (setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv)) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("setsockopt SO_SNDTIMEO failed");
        }
        return SOLO_SOCK_ERROR;
    }
    sock->timeout_ms = timeout_ms;
    return SOLO_SOCK_OK;
}

int solo_socket_wait(solo_socket_t *sock, int events, int timeout_ms)
{
    fd_set read_fds, write_fds, err_fds;
    struct timeval tv;
    struct timeval *ptv = NULL;
    int ret;
    int ready_events = 0;

    if (sock == NULL || sock->fd == INVALID_SOCKET) {
        return SOLO_SOCK_ERROR;
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&err_fds);

    if (events & SOLO_SOCK_POLL_READ) {
        FD_SET(sock->fd, &read_fds);
    }
    if (events & SOLO_SOCK_POLL_WRITE) {
        FD_SET(sock->fd, &write_fds);
    }
    FD_SET(sock->fd, &err_fds);

    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }

    ret = select(0, &read_fds, &write_fds, &err_fds, ptv);
    if (ret == SOCKET_ERROR) {
        return SOLO_SOCK_ERROR;
    }
    if (ret == 0) {
        return 0; /* Timeout */
    }

    if (FD_ISSET(sock->fd, &read_fds)) {
        ready_events |= SOLO_SOCK_POLL_READ;
    }
    if (FD_ISSET(sock->fd, &write_fds)) {
        ready_events |= SOLO_SOCK_POLL_WRITE;
    }
    if (FD_ISSET(sock->fd, &err_fds)) {
        ready_events |= SOLO_SOCK_POLL_ERR;
    }

    return ready_events;
}

int solo_socket_get_fd(const solo_socket_t *sock)
{
    return sock ? (int)sock->fd : -1;
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_windows_nt;
#endif /* SOLO_IS_WINDOWS_NT */
