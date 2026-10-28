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

#if SOLO_IS_WINDOWS_CE

#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>

#include <winsock.h>

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
        WSAStartup(MAKEWORD(1, 1), &wsa);
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

solo_socket_status_t solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char **err_msg)
{
    struct sockaddr_in sin;
    int opt = 1;

    if (sock == NULL || sock->fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

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

solo_socket_status_t solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char **err_msg)
{
    struct sockaddr_in sin;
    unsigned long ip;

    if (sock == NULL || sock->fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

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
}

solo_socket_t *solo_socket_connect(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    solo_socket_status_t res;
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
    struct sockaddr_in sin;
    int addr_len = sizeof(sin);
    SOCKET client_fd;
    solo_socket_t *client_sock;

    if (server == NULL || server->fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

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

solo_socket_status_t solo_socket_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    int total_sent = 0;
    int ret;

    if (sent != NULL) {
        *sent = 0;
    }
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
        if (sent != NULL) {
            *sent = (size_t)ret;
        }
        return SOLO_SOCK_OK;
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

    if (sent != NULL) {
        *sent = (size_t)total_sent;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len, size_t *received)
{
    int ret;
    if (received != NULL) {
        *received = 0;
    }
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
    if (ret == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
    }

    if (received != NULL) {
        *received = (size_t)ret;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_write(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    return solo_socket_send(sock, buf, len, sent);
}

solo_socket_status_t solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len, size_t *received)
{
    return solo_socket_recv(sock, buf, max_len, received);
}

solo_socket_status_t solo_socket_sendto(solo_socket_t *sock, const char *buf, size_t len,
                                        const char *host, int port, size_t *sent)
{
    struct sockaddr_in sin;
    unsigned long ip;
    int ret;

    if (sent != NULL) {
        *sent = 0;
    }
    if (sock == NULL || sock->fd == INVALID_SOCKET || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

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
    if (ret == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }

    if (sent != NULL) {
        *sent = (size_t)ret;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_recvfrom(solo_socket_t *sock, char *buf, size_t max_len,
                                          char *src_ip, size_t ip_len, int *src_port,
                                          size_t *received)
{
    struct sockaddr_in sin;
    int addr_len = sizeof(sin);
    int ret;

    if (received != NULL) {
        *received = 0;
    }
    if (sock == NULL || sock->fd == INVALID_SOCKET || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    ret = recvfrom(sock->fd, buf, (int)max_len, 0, (struct sockaddr *)&sin, &addr_len);
    if (ret == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    if (ret == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
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

    if (received != NULL) {
        *received = (size_t)ret;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
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

solo_socket_status_t solo_socket_set_timeout(solo_socket_t *sock, int timeout_ms, char **err_msg)
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
typedef int make_iso_compilers_happy_socket_windows_ce;
#endif /* SOLO_IS_WINDOWS_CE */
