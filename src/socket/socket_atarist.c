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

#if SOLO_IS_ATARIST

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <solo/socket.h>

struct solo_socket {
    int fd;
    solo_socket_type_t type;
    solo_socket_mode_t mode;
    int timeout_ms;
};

static char *format_socket_error(const char *op)
{
    char buf[128];
    if (errno == ENOSYS || errno == EPROTONOSUPPORT) {
        snprintf(buf, sizeof(buf), "%s: TCP/UDP networking requires FreeMiNT MintNet stack", op);
    } else {
        snprintf(buf, sizeof(buf), "%s failed: %s", op, strerror(errno));
    }
    return safe_strdup(buf);
}

static int resolve_address(const char *host, int port, struct sockaddr_in *sin, char **err_msg)
{
    unsigned long ip;
    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }
    memset(sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    sin->sin_port = htons((unsigned short)port);

    if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0) {
        sin->sin_addr.s_addr = htonl(INADDR_ANY);
        return 0;
    }

    ip = inet_addr(host);
    if (ip == (unsigned long)INADDR_NONE && strcmp(host, "255.255.255.255") != 0) {
        struct hostent *he = gethostbyname(host);
        if (he != NULL && he->h_addr_list != NULL && he->h_addr_list[0] != NULL) {
            memcpy(&sin->sin_addr, he->h_addr_list[0], sizeof(sin->sin_addr));
        } else {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("cannot resolve hostname");
            }
            return -1;
        }
    } else {
        sin->sin_addr.s_addr = ip;
    }
    return 0;
}

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char **err_msg)
{
    int socktype = (type == SOLO_SOCK_UDP) ? SOCK_DGRAM : SOCK_STREAM;
    int fd = socket(AF_INET, socktype, 0);
    solo_socket_t *sock;

    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = format_socket_error("socket()");
        }
        return NULL;
    }

    if (mode == SOLO_SOCK_NONBLOCKING) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        close(fd);
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
            close(sock->fd);
            sock->fd = -1;
        }
        free(sock);
    }
}

solo_socket_status_t solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char **err_msg)
{
    struct sockaddr_in sin;
    int opt = 1;

    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    if (resolve_address(host, port, &sin, err_msg) != 0) {
        return SOLO_SOCK_ERROR;
    }

    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sock->fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        if (err_msg != NULL) {
            *err_msg = format_socket_error("bind()");
        }
        return SOLO_SOCK_ERROR;
    }

    if (sock->type == SOLO_SOCK_TCP) {
        if (listen(sock->fd, backlog > 0 ? backlog : 5) < 0) {
            if (err_msg != NULL) {
                *err_msg = format_socket_error("listen()");
            }
            return SOLO_SOCK_ERROR;
        }
    }

    return SOLO_SOCK_OK;
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, solo_socket_mode_t mode, char **err_msg)
{
    solo_socket_t *sock = solo_socket_create(SOLO_SOCK_TCP, mode, err_msg);
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
    solo_socket_t *sock = solo_socket_create(SOLO_SOCK_UDP, mode, err_msg);
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

    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    if (resolve_address(host, port, &sin, err_msg) != 0) {
        return SOLO_SOCK_ERROR;
    }

    if (connect(sock->fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        if (sock->mode == SOLO_SOCK_NONBLOCKING && (errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EAGAIN)) {
            return SOLO_SOCK_AGAIN;
        }
        if (err_msg != NULL) {
            *err_msg = format_socket_error("connect()");
        }
        return SOLO_SOCK_ERROR;
    }

    return SOLO_SOCK_OK;
}

solo_socket_t *solo_socket_connect(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    solo_socket_status_t status;
    solo_socket_t *sock = solo_socket_create(SOLO_SOCK_TCP, mode, err_msg);
    if (sock == NULL) {
        return NULL;
    }

    status = solo_socket_connect_sock(sock, host, port, err_msg);
    if (status != SOLO_SOCK_OK && status != SOLO_SOCK_AGAIN) {
        solo_socket_close(sock);
        return NULL;
    }

    return sock;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                  int *client_port, char **err_msg)
{
    struct sockaddr_in sin;
    socklen_t addrlen = sizeof(sin);
    int client_fd;
    solo_socket_t *client_sock;

    if (server == NULL || server->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

    memset(&sin, 0, sizeof(sin));
    client_fd = accept(server->fd, (struct sockaddr *)&sin, &addrlen);
    if (client_fd < 0) {
        if (server->mode == SOLO_SOCK_NONBLOCKING && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("resource temporarily unavailable");
            }
            return NULL;
        }
        if (err_msg != NULL) {
            *err_msg = format_socket_error("accept()");
        }
        return NULL;
    }

    if (server->mode == SOLO_SOCK_NONBLOCKING) {
        int flags = fcntl(client_fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    if (client_ip != NULL && ip_len > 0) {
        char *ip_str = inet_ntoa(sin.sin_addr);
        if (ip_str != NULL) {
            strncpy(client_ip, ip_str, ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        } else {
            client_ip[0] = '\0';
        }
    }

    if (client_port != NULL) {
        *client_port = ntohs(sin.sin_port);
    }

    client_sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (client_sock == NULL) {
        close(client_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }

    client_sock->fd = client_fd;
    client_sock->type = SOLO_SOCK_TCP;
    client_sock->mode = server->mode;
    client_sock->timeout_ms = server->timeout_ms;
    return client_sock;
}

solo_socket_status_t solo_socket_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    int ret;
    if (sent != NULL) {
        *sent = 0;
    }
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }
    ret = send(sock->fd, buf, len, 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    if (sent != NULL) {
        *sent = (size_t)ret;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len, size_t *received)
{
    int n;
    if (received != NULL) {
        *received = 0;
    }
    if (sock == NULL || sock->fd < 0 || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }
    n = recv(sock->fd, buf, max_len, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    if (n == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
    }
    if (received != NULL) {
        *received = (size_t)n;
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
    int ret;

    if (sent != NULL) {
        *sent = 0;
    }
    if (sock == NULL || sock->fd < 0 || buf == NULL || host == NULL) {
        return SOLO_SOCK_ERROR;
    }

    if (resolve_address(host, port, &sin, NULL) != 0) {
        return SOLO_SOCK_ERROR;
    }

    ret = sendto(sock->fd, buf, len, 0, (struct sockaddr *)&sin, sizeof(sin));
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
    socklen_t sin_len = sizeof(sin);
    int n;

    if (received != NULL) {
        *received = 0;
    }
    if (sock == NULL || sock->fd < 0 || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    memset(&sin, 0, sizeof(sin));
    n = recvfrom(sock->fd, buf, max_len, 0, (struct sockaddr *)&sin, &sin_len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    if (n == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
    }

    if (src_ip != NULL && ip_len > 0) {
        char *ip_str = inet_ntoa(sin.sin_addr);
        if (ip_str != NULL) {
            strncpy(src_ip, ip_str, ip_len - 1);
            src_ip[ip_len - 1] = '\0';
        } else {
            src_ip[0] = '\0';
        }
    }

    if (src_port != NULL) {
        *src_port = ntohs(sin.sin_port);
    }

    if (received != NULL) {
        *received = (size_t)n;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    int flags;
    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    flags = fcntl(sock->fd, F_GETFL, 0);
    if (flags < 0) {
        flags = 0;
    }

    if (mode == SOLO_SOCK_NONBLOCKING) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(sock->fd, F_SETFL, flags) < 0) {
        if (err_msg != NULL) {
            *err_msg = format_socket_error("fcntl(F_SETFL)");
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
    struct timeval tv;
    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    sock->timeout_ms = timeout_ms;
    if (timeout_ms < 0) {
        timeout_ms = 0;
    }
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    return SOLO_SOCK_OK;
}

int solo_socket_wait(solo_socket_t *sock, int events, int timeout_ms)
{
    fd_set read_fds, write_fds, except_fds;
    struct timeval tv, *tvp = NULL;
    int max_fd;
    int res = 0;
    int rc;

    if (sock == NULL || sock->fd < 0) {
        return SOLO_SOCK_ERROR;
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);

    if (events & SOLO_SOCK_POLL_READ) {
        FD_SET(sock->fd, &read_fds);
    }
    if (events & SOLO_SOCK_POLL_WRITE) {
        FD_SET(sock->fd, &write_fds);
    }
    if (events & SOLO_SOCK_POLL_ERR) {
        FD_SET(sock->fd, &except_fds);
    }

    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tvp = &tv;
    }

    max_fd = sock->fd + 1;
    rc = select(max_fd, &read_fds, &write_fds, &except_fds, tvp);
    if (rc < 0) {
        if (errno == EINTR) {
            return 0;
        }
        return SOLO_SOCK_ERROR;
    }
    if (rc == 0) {
        return 0;
    }

    if (FD_ISSET(sock->fd, &read_fds)) {
        res |= SOLO_SOCK_POLL_READ;
    }
    if (FD_ISSET(sock->fd, &write_fds)) {
        res |= SOLO_SOCK_POLL_WRITE;
    }
    if (FD_ISSET(sock->fd, &except_fds)) {
        res |= SOLO_SOCK_POLL_ERR;
    }

    return res;
}

int solo_socket_get_fd(const solo_socket_t *sock)
{
    return sock ? sock->fd : -1;
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_atarist;
#endif /* SOLO_IS_ATARIST */
