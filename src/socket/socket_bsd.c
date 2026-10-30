/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "socket_pal.h"
#include "socket_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SOLO_SOCKLEN_T
#if SOLO_IS_WINDOWS || SOLO_IS_OS2 || SOLO_IS_DOS
#define SOLO_SOCKLEN_T int
#elif SOLO_IS_AMIGA
#define SOLO_SOCKLEN_T unsigned long
#else
#define SOLO_SOCKLEN_T socklen_t
#endif
#endif

/*
 * Resolves an IPv4 address/hostname and port into a struct sockaddr_in.
 */
static int solo_pal_resolve_ipv4(const char *host, int port, struct sockaddr_in *sin, char *err_buf, size_t err_cap)
{
    unsigned long ip_be = 0;

    memset(sin, 0, sizeof(*sin));
    sin->sin_family = AF_INET;
    sin->sin_port = htons((unsigned short)port);

    if (host == NULL || host[0] == '\0' || strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0) {
        sin->sin_addr.s_addr = htonl(INADDR_ANY);
        return 0;
    }

    if (strcmp(host, "255.255.255.255") == 0) {
        sin->sin_addr.s_addr = 0xFFFFFFFFUL;
        return 0;
    }

    if (solo_socket_parse_ipv4(host, &ip_be)) {
        sin->sin_addr.s_addr = htonl(ip_be);
        return 0;
    }

    /* Hostname lookup via DNS / hosts */
    {
        struct hostent *he = gethostbyname(host);
        if (he != NULL && he->h_addr_list != NULL && he->h_addr_list[0] != NULL) {
            memcpy(&sin->sin_addr, he->h_addr_list[0], sizeof(sin->sin_addr));
            return 0;
        }
    }

    solo_set_err_fmt(err_buf, err_cap, "cannot resolve host '%.128s'", host);
    return -1;
}

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char *err_buf, size_t err_cap)
{
    int socktype = (type == SOLO_SOCK_UDP) ? SOCK_DGRAM : SOCK_STREAM;
    SOLO_PAL_SOCK_T fd;
    solo_socket_t *sock;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (!solo_pal_sock_init(err_buf, err_cap)) {
        return NULL;
    }

    fd = socket(AF_INET, socktype, 0);
    if (fd == SOLO_PAL_INVALID_SOCK) {
        solo_pal_strerror(SOLO_PAL_SOCK_ERRNO, err_buf, err_cap);
        return NULL;
    }

    if (mode == SOLO_SOCK_NONBLOCKING) {
        if (solo_pal_set_nonblocking(fd, 1) != 0) {
            solo_pal_strerror(SOLO_PAL_SOCK_ERRNO, err_buf, err_cap);
            SOLO_PAL_CLOSESOCKET(fd);
            return NULL;
        }
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        SOLO_PAL_CLOSESOCKET(fd);
        solo_set_err(err_buf, err_cap, "out of memory allocating socket structure");
        return NULL;
    }

    sock->fd = fd;
    sock->type = type;
    sock->mode = mode;
    sock->timeout_ms = 0;
    return sock;
}

void solo_socket_backend_close(solo_socket_t *sock)
{
    if (sock != NULL && SOLO_SOCK_IS_VALID(sock)) {
        SOLO_PAL_CLOSESOCKET(sock->fd);
        sock->fd = SOLO_PAL_INVALID_SOCK;
    }
}

solo_socket_status_t solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char *err_buf, size_t err_cap)
{
    struct sockaddr_in sin;
    int opt = 1;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (sock == NULL || !SOLO_SOCK_IS_VALID(sock)) {
        solo_set_err(err_buf, err_cap, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }

    host = solo_socket_normalize_bind_host(host);
    if (solo_pal_resolve_ipv4(host, port, &sin, err_buf, err_cap) != 0) {
        return SOLO_SOCK_ERROR;
    }

    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    if (bind(sock->fd, (struct sockaddr *)&sin, (SOLO_SOCKLEN_T)sizeof(sin)) != 0) {
        solo_pal_strerror(SOLO_PAL_SOCK_ERRNO, err_buf, err_cap);
        return SOLO_SOCK_ERROR;
    }

    if (sock->type == SOLO_SOCK_TCP) {
        if (listen(sock->fd, backlog > 0 ? backlog : 128) != 0) {
            solo_pal_strerror(SOLO_PAL_SOCK_ERRNO, err_buf, err_cap);
            return SOLO_SOCK_ERROR;
        }
    }

    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char *err_buf, size_t err_cap)
{
    struct sockaddr_in sin;
    int res;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (sock == NULL || !SOLO_SOCK_IS_VALID(sock)) {
        solo_set_err(err_buf, err_cap, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }

    host = solo_socket_normalize_connect_host(host);
    if (solo_pal_resolve_ipv4(host, port, &sin, err_buf, err_cap) != 0) {
        return SOLO_SOCK_ERROR;
    }

    res = connect(sock->fd, (struct sockaddr *)&sin, (SOLO_SOCKLEN_T)sizeof(sin));
    if (res != 0) {
        int err = SOLO_PAL_SOCK_ERRNO;
        if (SOLO_PAL_IS_INPROGRESS(err) || SOLO_PAL_IS_WOULDBLOCK(err)) {
            return SOLO_SOCK_AGAIN;
        }
        solo_pal_strerror(err, err_buf, err_cap);
        return SOLO_SOCK_ERROR;
    }

    return SOLO_SOCK_OK;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                  int *client_port, char *err_buf, size_t err_cap)
{
    struct sockaddr_in addr;
    SOLO_SOCKLEN_T addr_len = (SOLO_SOCKLEN_T)sizeof(addr);
    SOLO_PAL_SOCK_T client_fd;
    solo_socket_t *client_sock;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (server == NULL || !SOLO_SOCK_IS_VALID(server)) {
        solo_set_err(err_buf, err_cap, "invalid server socket");
        return NULL;
    }

    client_fd = accept(server->fd, (struct sockaddr *)&addr, &addr_len);
    if (client_fd == SOLO_PAL_INVALID_SOCK) {
        int err = SOLO_PAL_SOCK_ERRNO;
        if (SOLO_PAL_IS_WOULDBLOCK(err)) {
            return NULL;
        }
        solo_pal_strerror(err, err_buf, err_cap);
        return NULL;
    }

    if (client_ip != NULL && ip_len > 0) {
        unsigned long ip_h = ntohl(addr.sin_addr.s_addr);
        solo_socket_format_ipv4(ip_h, client_ip, ip_len);
        if (client_port != NULL) {
            *client_port = (int)ntohs(addr.sin_port);
        }
    }

    client_sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (client_sock == NULL) {
        SOLO_PAL_CLOSESOCKET(client_fd);
        solo_set_err(err_buf, err_cap, "out of memory allocating client socket");
        return NULL;
    }

    client_sock->fd = client_fd;
    client_sock->type = SOLO_SOCK_TCP;
    client_sock->mode = server->mode;
    client_sock->timeout_ms = 0;

    if (server->mode == SOLO_SOCK_NONBLOCKING) {
        solo_pal_set_nonblocking(client_fd, 1);
    }

    return client_sock;
}

solo_socket_status_t solo_socket_backend_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    int n;
    if (sent != NULL) {
        *sent = 0;
    }
    if (sock == NULL || !SOLO_SOCK_IS_VALID(sock) || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

#if defined(MSG_NOSIGNAL)
    n = send(sock->fd, buf, (int)len, MSG_NOSIGNAL);
#else
    n = send(sock->fd, buf, (int)len, 0);
#endif

    if (n < 0) {
        int err = SOLO_PAL_SOCK_ERRNO;
        if (SOLO_PAL_IS_INTR(err) || SOLO_PAL_IS_WOULDBLOCK(err)) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }

    if (sent != NULL) {
        *sent = (size_t)n;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len, size_t *received)
{
    int n;
    if (received != NULL) {
        *received = 0;
    }
    if (sock == NULL || !SOLO_SOCK_IS_VALID(sock) || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    while (1) {
        n = recv(sock->fd, buf, (int)max_len, 0);
        if (n < 0) {
            int err = SOLO_PAL_SOCK_ERRNO;
            if (SOLO_PAL_IS_INTR(err)) {
                continue;
            }
            if (SOLO_PAL_IS_WOULDBLOCK(err)) {
                return SOLO_SOCK_AGAIN;
            }
            return SOLO_SOCK_ERROR;
        }
        break;
    }

    if (n == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
    }

    if (received != NULL) {
        *received = (size_t)n;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_sendto(solo_socket_t *sock, const char *buf, size_t len,
                                        const char *host, int port, size_t *sent)
{
    struct sockaddr_in sin;
    int n;

    if (sent != NULL) {
        *sent = 0;
    }
    if (sock == NULL || !SOLO_SOCK_IS_VALID(sock) || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

    host = solo_socket_normalize_connect_host(host);
    if (solo_pal_resolve_ipv4(host, port, &sin, NULL, 0) != 0) {
        return SOLO_SOCK_ERROR;
    }

    while (1) {
        n = sendto(sock->fd, buf, (int)len, 0, (struct sockaddr *)&sin, (SOLO_SOCKLEN_T)sizeof(sin));
        if (n < 0) {
            int err = SOLO_PAL_SOCK_ERRNO;
            if (SOLO_PAL_IS_INTR(err)) {
                continue;
            }
            if (SOLO_PAL_IS_WOULDBLOCK(err)) {
                return SOLO_SOCK_AGAIN;
            }
            return SOLO_SOCK_ERROR;
        }
        break;
    }

    if (sent != NULL) {
        *sent = (size_t)n;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_recvfrom(solo_socket_t *sock, char *buf, size_t max_len,
                                          char *src_ip, size_t ip_len, int *src_port,
                                          size_t *received)
{
    struct sockaddr_in sin;
    SOLO_SOCKLEN_T addr_len = (SOLO_SOCKLEN_T)sizeof(sin);
    int n;

    if (received != NULL) {
        *received = 0;
    }
    if (sock == NULL || !SOLO_SOCK_IS_VALID(sock) || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    while (1) {
        n = recvfrom(sock->fd, buf, (int)max_len, 0, (struct sockaddr *)&sin, &addr_len);
        if (n < 0) {
            int err = SOLO_PAL_SOCK_ERRNO;
            if (SOLO_PAL_IS_INTR(err)) {
                continue;
            }
            if (SOLO_PAL_IS_WOULDBLOCK(err)) {
                return SOLO_SOCK_AGAIN;
            }
            return SOLO_SOCK_ERROR;
        }
        break;
    }

    if (n == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
    }

    if (src_ip != NULL && ip_len > 0) {
        unsigned long ip_h = ntohl(sin.sin_addr.s_addr);
        solo_socket_format_ipv4(ip_h, src_ip, ip_len);
        if (src_port != NULL) {
            *src_port = (int)ntohs(sin.sin_port);
        }
    }

    if (received != NULL) {
        *received = (size_t)n;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_backend_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char *err_buf, size_t err_cap)
{
    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (sock == NULL || !SOLO_SOCK_IS_VALID(sock)) {
        solo_set_err(err_buf, err_cap, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }

    if (solo_pal_set_nonblocking(sock->fd, (mode == SOLO_SOCK_NONBLOCKING) ? 1 : 0) != 0) {
        solo_pal_strerror(SOLO_PAL_SOCK_ERRNO, err_buf, err_cap);
        return SOLO_SOCK_ERROR;
    }

    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_backend_set_timeout(solo_socket_t *sock, int timeout_ms, char *err_buf, size_t err_cap)
{
    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (sock == NULL || !SOLO_SOCK_IS_VALID(sock)) {
        solo_set_err(err_buf, err_cap, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }

    if (solo_pal_set_timeout(sock->fd, timeout_ms) != 0) {
        solo_pal_strerror(SOLO_PAL_SOCK_ERRNO, err_buf, err_cap);
        return SOLO_SOCK_ERROR;
    }

    return SOLO_SOCK_OK;
}

int solo_socket_backend_poll(solo_pollfd_t *fds, size_t nfds, int timeout_ms)
{
#if defined(SOLO_PAL_HAS_CUSTOM_POLL)
    return solo_pal_custom_poll(fds, nfds, timeout_ms);
#else
    fd_set read_fds, write_fds, err_fds;
    SOLO_PAL_SOCK_T max_fd = 0;
    struct timeval tv;
    struct timeval *ptv = NULL;
    size_t i;
    int ret;
    int ready_count = 0;

    if (fds == NULL && nfds > 0) {
        return SOLO_SOCK_ERROR;
    }
    if (nfds == 0) {
        if (timeout_ms > 0) {
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000L;
            SOLO_PAL_SELECT(0, NULL, NULL, NULL, &tv);
        }
        return 0;
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&err_fds);

    for (i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].sock != NULL && SOLO_SOCK_IS_VALID(fds[i].sock)) {
            SOLO_PAL_SOCK_T s = fds[i].sock->fd;
            if (fds[i].events & SOLO_SOCK_POLL_READ) {
                FD_SET(s, &read_fds);
            }
            if (fds[i].events & SOLO_SOCK_POLL_WRITE) {
                FD_SET(s, &write_fds);
            }
            FD_SET(s, &err_fds);
            if (s > max_fd) {
                max_fd = s;
            }
        }
    }

    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000L;
        ptv = &tv;
    }

    while (1) {
        ret = SOLO_PAL_SELECT(max_fd + 1, &read_fds, &write_fds, &err_fds, ptv);
        if (ret < 0) {
            int err = SOLO_PAL_SOCK_ERRNO;
            if (SOLO_PAL_IS_INTR(err)) {
                continue;
            }
            return SOLO_SOCK_ERROR;
        }
        break;
    }

    if (ret > 0) {
        for (i = 0; i < nfds; i++) {
            if (fds[i].sock != NULL && SOLO_SOCK_IS_VALID(fds[i].sock)) {
                SOLO_PAL_SOCK_T s = fds[i].sock->fd;
                if (FD_ISSET(s, &read_fds)) {
                    fds[i].revents |= SOLO_SOCK_POLL_READ;
                }
                if (FD_ISSET(s, &write_fds)) {
                    fds[i].revents |= SOLO_SOCK_POLL_WRITE;
                }
                if (FD_ISSET(s, &err_fds)) {
                    fds[i].revents |= SOLO_SOCK_POLL_ERR;
                }
                if (fds[i].revents != 0) {
                    ready_count++;
                }
            }
        }
    }

    return ready_count;
#endif
}
