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

#if SOLO_IS_DOS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>

#if defined(USE_WATT32)
#include <tcp.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#ifndef FIONBIO
#define FIONBIO 0x8004667e
#endif
#endif

struct solo_socket {
    int fd;
    solo_socket_type_t type;
    solo_socket_mode_t mode;
    int timeout_ms;
};

#if defined(USE_WATT32)

static int watt_init_done = 0;

static int ensure_watt32_init(char **err_msg)
{
    if (!watt_init_done) {
        int rc = sock_init();
        if (rc != 0) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup(sock_init_err(rc));
            }
            return 0;
        }
        watt_init_done = 1;
    }
    return 1;
}

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char **err_msg)
{
    int socktype = (type == SOLO_SOCK_UDP) ? SOCK_DGRAM : SOCK_STREAM;
    int fd;
    solo_socket_t *sock;

    if (!ensure_watt32_init(err_msg)) {
        return NULL;
    }

    fd = socket(AF_INET, socktype, 0);
    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("WATT-32: failed to create socket");
        }
        return NULL;
    }

    if (mode == SOLO_SOCK_NONBLOCKING) {
        int nb = 1;
        ioctlsocket(fd, FIONBIO, (char *)&nb);
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        close_s(fd);
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
    if (sock != NULL) {
        if (sock->fd >= 0) {
            close_s(sock->fd);
            sock->fd = -1;
        }
        free(sock);
    }
}

int solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char **err_msg)
{
    struct sockaddr_in sin;
    int opt = 1;

    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    if (host == NULL || strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0 || host[0] == '\0') {
        sin.sin_addr.s_addr = INADDR_ANY;
    } else {
        sin.sin_addr.s_addr = inet_addr((char *)host);
    }

    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    if (bind(sock->fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("WATT-32: failed to bind socket");
        }
        return SOLO_SOCK_ERROR;
    }

    if (sock->type == SOLO_SOCK_TCP) {
        if (listen(sock->fd, backlog > 0 ? backlog : 128) != 0) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("WATT-32: failed to listen on socket");
            }
            return SOLO_SOCK_ERROR;
        }
    }

    return SOLO_SOCK_OK;
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, solo_socket_mode_t mode, char **err_msg)
{
    solo_socket_t *sock = solo_socket_tcp(mode, err_msg);
    if (sock == NULL) return NULL;
    if (solo_socket_bind_sock(sock, host, port, backlog, err_msg) != SOLO_SOCK_OK) {
        solo_socket_close(sock);
        return NULL;
    }
    return sock;
}

solo_socket_t *solo_socket_udp_bind(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    solo_socket_t *sock = solo_socket_udp(mode, err_msg);
    if (sock == NULL) return NULL;
    if (solo_socket_bind_sock(sock, host, port, 0, err_msg) != SOLO_SOCK_OK) {
        solo_socket_close(sock);
        return NULL;
    }
    return sock;
}

int solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char **err_msg)
{
    struct sockaddr_in sin;

    if (sock == NULL || sock->fd < 0) {
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
    sin.sin_addr.s_addr = inet_addr((char *)host);

    if (sin.sin_addr.s_addr == INADDR_NONE) {
        struct hostent *he = gethostbyname((char *)host);
        if (he == NULL || he->h_addr == NULL) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("WATT-32: cannot resolve host name");
            }
            return SOLO_SOCK_ERROR;
        }
        memcpy(&sin.sin_addr, he->h_addr, he->h_length);
    }

    if (connect(sock->fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        if (err_msg != NULL) {
            *err_msg = safe_strdup("WATT-32: failed to connect to host");
        }
        return SOLO_SOCK_ERROR;
    }

    return SOLO_SOCK_OK;
}

solo_socket_t *solo_socket_connect(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    int res;
    solo_socket_t *sock = solo_socket_tcp(mode, err_msg);
    if (sock == NULL) return NULL;
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
    int client_fd;
    solo_socket_t *client_sock;

    if (server == NULL || server->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

    client_fd = accept(server->fd, (struct sockaddr *)&sin, &addr_len);
    if (client_fd < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
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
        char *ip = inet_ntoa(sin.sin_addr);
        if (ip != NULL) {
            strncpy(client_ip, ip, ip_len - 1);
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
        close_s(client_fd);
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
        int nb = 1;
        ioctlsocket(client_fd, FIONBIO, (char *)&nb);
    }

    return client_sock;
}

int solo_socket_send(solo_socket_t *sock, const char *buf, size_t len)
{
    int ret;
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

    ret = send(sock->fd, (char *)buf, len, 0);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    return ret;
}

int solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len)
{
    int ret;
    if (sock == NULL || sock->fd < 0 || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    ret = recv(sock->fd, buf, max_len, 0);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
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
    struct sockaddr_in sin;
    int ret;

    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);
    sin.sin_addr.s_addr = inet_addr((char *)host);

    ret = sendto(sock->fd, (char *)buf, len, 0, (struct sockaddr *)&sin, sizeof(sin));
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    return ret;
}

int solo_socket_recvfrom(solo_socket_t *sock, char *buf, size_t max_len,
                         char *src_ip, size_t ip_len, int *src_port)
{
    struct sockaddr_in sin;
    int addr_len = sizeof(sin);
    int n;

    if (sock == NULL || sock->fd < 0 || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    n = recvfrom(sock->fd, buf, max_len, 0, (struct sockaddr *)&sin, &addr_len);
    if (n < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }

    if (src_ip != NULL && ip_len > 0) {
        char *ip = inet_ntoa(sin.sin_addr);
        if (ip != NULL) {
            strncpy(src_ip, ip, ip_len - 1);
            src_ip[ip_len - 1] = '\0';
        } else {
            strncpy(src_ip, "unknown", ip_len - 1);
            src_ip[ip_len - 1] = '\0';
        }
    }
    if (src_port != NULL) {
        *src_port = (int)ntohs(sin.sin_port);
    }

    return n;
}

int solo_socket_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    int nb;
    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    nb = (mode == SOLO_SOCK_NONBLOCKING) ? 1 : 0;
    if (ioctlsocket(sock->fd, FIONBIO, (char *)&nb) < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("ioctlsocket FIONBIO failed");
        }
        return SOLO_SOCK_ERROR;
    }

    sock->mode = mode;
    return SOLO_SOCK_OK;
}

int solo_socket_set_timeout(solo_socket_t *sock, int timeout_ms, char **err_msg)
{
    struct timeval tv;
    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    if (timeout_ms < 0) timeout_ms = 0;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));

    sock->timeout_ms = timeout_ms;
    return SOLO_SOCK_OK;
}

int solo_socket_wait(solo_socket_t *sock, int events, int timeout_ms)
{
    fd_set read_fds, write_fds, err_fds;
    struct timeval tv;
    struct timeval *ptv = NULL;
    int ret;
    int ready = 0;

    if (sock == NULL || sock->fd < 0) {
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

    ret = select_s(sock->fd + 1, &read_fds, &write_fds, &err_fds, ptv);
    if (ret < 0) {
        return SOLO_SOCK_ERROR;
    }
    if (ret == 0) {
        return 0; /* Timeout */
    }

    if (FD_ISSET(sock->fd, &read_fds)) ready |= SOLO_SOCK_POLL_READ;
    if (FD_ISSET(sock->fd, &write_fds)) ready |= SOLO_SOCK_POLL_WRITE;
    if (FD_ISSET(sock->fd, &err_fds)) ready |= SOLO_SOCK_POLL_ERR;

    return ready;
}

#else /* !defined(USE_WATT32) */

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char **err_msg)
{
    (void)type; (void)mode;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("DOS socket requires WATT-32 (compile with -DUSE_WATT32)");
    }
    return NULL;
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
    (void)sock;
}

solo_socket_t *solo_socket_connect(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    (void)host; (void)port;
    return solo_socket_create(SOLO_SOCK_TCP, mode, err_msg);
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, solo_socket_mode_t mode, char **err_msg)
{
    (void)host; (void)port; (void)backlog;
    return solo_socket_create(SOLO_SOCK_TCP, mode, err_msg);
}

solo_socket_t *solo_socket_udp_bind(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    (void)host; (void)port;
    return solo_socket_create(SOLO_SOCK_UDP, mode, err_msg);
}

int solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char **err_msg)
{
    (void)sock; (void)host; (void)port; (void)backlog;
    if (err_msg) *err_msg = safe_strdup("not supported without WATT-32");
    return SOLO_SOCK_ERROR;
}

int solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char **err_msg)
{
    (void)sock; (void)host; (void)port;
    if (err_msg) *err_msg = safe_strdup("not supported without WATT-32");
    return SOLO_SOCK_ERROR;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                  int *client_port, char **err_msg)
{
    (void)server; (void)client_ip; (void)ip_len; (void)client_port;
    if (err_msg) *err_msg = safe_strdup("not supported without WATT-32");
    return NULL;
}

int solo_socket_send(solo_socket_t *sock, const char *buf, size_t len)
{
    (void)sock; (void)buf; (void)len;
    return SOLO_SOCK_ERROR;
}

int solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len)
{
    (void)sock; (void)buf; (void)max_len;
    return SOLO_SOCK_ERROR;
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
    (void)sock; (void)buf; (void)len; (void)host; (void)port;
    return SOLO_SOCK_ERROR;
}

int solo_socket_recvfrom(solo_socket_t *sock, char *buf, size_t max_len,
                         char *src_ip, size_t ip_len, int *src_port)
{
    (void)sock; (void)buf; (void)max_len; (void)src_ip; (void)ip_len; (void)src_port;
    return SOLO_SOCK_ERROR;
}

int solo_socket_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    (void)sock; (void)mode; (void)err_msg;
    return SOLO_SOCK_OK;
}

int solo_socket_set_timeout(solo_socket_t *sock, int timeout_ms, char **err_msg)
{
    (void)sock; (void)timeout_ms; (void)err_msg;
    return SOLO_SOCK_OK;
}

int solo_socket_wait(solo_socket_t *sock, int events, int timeout_ms)
{
    (void)sock; (void)events; (void)timeout_ms;
    return 0;
}

#endif /* USE_WATT32 */

solo_socket_mode_t solo_socket_get_mode(const solo_socket_t *sock)
{
    return sock ? sock->mode : SOLO_SOCK_BLOCKING;
}

solo_socket_type_t solo_socket_get_type(const solo_socket_t *sock)
{
    return sock ? sock->type : SOLO_SOCK_TCP;
}

int solo_socket_get_fd(const solo_socket_t *sock)
{
    return sock ? sock->fd : -1;
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_dos;
#endif /* SOLO_IS_DOS */
