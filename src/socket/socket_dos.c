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
#include "socket_internal.h"

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

void solo_socket_backend_close(solo_socket_t *sock)
{
    if (sock != NULL && sock->fd >= 0) {
        close_s(sock->fd);
        sock->fd = -1;
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

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    host = solo_socket_normalize_bind_host(host);
    if (host == NULL) {
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

solo_socket_status_t solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char **err_msg)
{
    struct sockaddr_in sin;
    unsigned long ip;

    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    host = solo_socket_normalize_connect_host(host);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    ip = inet_addr((char *)host);
    if (ip == (unsigned long)INADDR_NONE && strcmp(host, "255.255.255.255") != 0) {
        ip = lookup_host(host, NULL);
        if (ip == 0) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("WATT-32: cannot resolve hostname");
            }
            return SOLO_SOCK_ERROR;
        }
    }
    sin.sin_addr.s_addr = ip;

    if (connect(sock->fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        if (errno == EWOULDBLOCK || errno == EINPROGRESS) {
            return SOLO_SOCK_AGAIN;
        }
        if (err_msg != NULL) {
            *err_msg = safe_strdup("WATT-32: failed to connect");
        }
        return SOLO_SOCK_ERROR;
    }

    return SOLO_SOCK_OK;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                  int *client_port, char **err_msg)
{
    struct sockaddr_in sin;
    int addrlen = sizeof(sin);
    int client_fd;
    solo_socket_t *client_sock;

    if (server == NULL || server->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

    client_fd = accept(server->fd, (struct sockaddr *)&sin, &addrlen);
    if (client_fd < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return NULL;
        }
        if (err_msg != NULL) {
            *err_msg = safe_strdup("WATT-32: accept failed");
        }
        return NULL;
    }

    if (client_ip != NULL && ip_len > 0) {
        char *ip_str = inet_ntoa(sin.sin_addr);
        if (ip_str != NULL) {
            strncpy(client_ip, ip_str, ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        } else {
            strncpy(client_ip, "unknown", ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        }
        if (client_port != NULL) {
            *client_port = ntohs(sin.sin_port);
        }
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

solo_socket_status_t solo_socket_backend_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    int ret;
    if (sent != NULL) {
        *sent = 0;
    }
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

    ret = send(sock->fd, (char *)buf, len, 0);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
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
    int ret;
    if (received != NULL) {
        *received = 0;
    }
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
    if (ret == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
    }

    if (received != NULL) {
        *received = (size_t)ret;
    }
    return SOLO_SOCK_OK;
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
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

    host = solo_socket_normalize_connect_host(host);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    ip = inet_addr((char *)host);
    if (ip == (unsigned long)INADDR_NONE && strcmp(host, "255.255.255.255") != 0) {
        ip = lookup_host(host, NULL);
        if (ip == 0) {
            return SOLO_SOCK_ERROR;
        }
    }
    sin.sin_addr.s_addr = ip;

    ret = sendto(sock->fd, (char *)buf, len, 0, (struct sockaddr *)&sin, sizeof(sin));
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
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
    int addrlen = sizeof(sin);
    int ret;

    if (received != NULL) {
        *received = 0;
    }
    if (sock == NULL || sock->fd < 0 || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    ret = recvfrom(sock->fd, buf, max_len, 0, (struct sockaddr *)&sin, &addrlen);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    if (ret == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
    }

    if (src_ip != NULL && ip_len > 0) {
        char *ip_str = inet_ntoa(sin.sin_addr);
        if (ip_str != NULL) {
            strncpy(src_ip, ip_str, ip_len - 1);
            src_ip[ip_len - 1] = '\0';
        } else {
            strncpy(src_ip, "unknown", ip_len - 1);
            src_ip[ip_len - 1] = '\0';
        }
        if (src_port != NULL) {
            *src_port = ntohs(sin.sin_port);
        }
    }

    if (received != NULL) {
        *received = (size_t)ret;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_backend_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    int nb;
    if (sock == NULL || sock->fd < 0) {
        solo_socket_set_err(err_msg, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }
    nb = (mode == SOLO_SOCK_NONBLOCKING) ? 1 : 0;
    ioctlsocket(sock->fd, FIONBIO, (char *)&nb);
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_backend_set_timeout(solo_socket_t *sock, int timeout_ms, char **err_msg)
{
    struct timeval tv;
    if (sock == NULL || sock->fd < 0) {
        solo_socket_set_err(err_msg, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }
    solo_socket_ms_to_timeval(timeout_ms, &tv.tv_sec, &tv.tv_usec);
    setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
    return SOLO_SOCK_OK;
}

int solo_socket_backend_poll(solo_pollfd_t *fds, size_t nfds, int timeout_ms)
{
    fd_set read_fds, write_fds, err_fds;
    struct timeval tv;
    struct timeval *ptv = NULL;
    int max_fd = -1;
    int ret;
    size_t i;
    int ready_count = 0;

    if (fds == NULL || nfds == 0) {
        return 0;
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&err_fds);

    for (i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].sock != NULL && fds[i].sock->fd >= 0) {
            int fd = fds[i].sock->fd;
            if (fds[i].events & SOLO_SOCK_POLL_READ) {
                FD_SET(fd, &read_fds);
            }
            if (fds[i].events & SOLO_SOCK_POLL_WRITE) {
                FD_SET(fd, &write_fds);
            }
            FD_SET(fd, &err_fds);
            if (fd > max_fd) {
                max_fd = fd;
            }
        }
    }

    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }

    ret = select_s(max_fd + 1, &read_fds, &write_fds, &err_fds, ptv);
    if (ret < 0) {
        return SOLO_SOCK_ERROR;
    }
    if (ret == 0) {
        return 0;
    }

    for (i = 0; i < nfds; i++) {
        if (fds[i].sock != NULL && fds[i].sock->fd >= 0) {
            int fd = fds[i].sock->fd;
            if (FD_ISSET(fd, &read_fds)) {
                fds[i].revents |= SOLO_SOCK_POLL_READ;
            }
            if (FD_ISSET(fd, &write_fds)) {
                fds[i].revents |= SOLO_SOCK_POLL_WRITE;
            }
            if (FD_ISSET(fd, &err_fds)) {
                fds[i].revents |= SOLO_SOCK_POLL_ERR;
            }
            if (fds[i].revents != 0) {
                ready_count++;
            }
        }
    }

    return ready_count;
}

#else /* !defined(USE_WATT32) */

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char **err_msg)
{
    (void)type; (void)mode;
    solo_socket_set_err(err_msg, "DOS socket requires WATT-32 (compile with -DUSE_WATT32)");
    return NULL;
}

void solo_socket_backend_close(solo_socket_t *sock)
{
    (void)sock;
}

solo_socket_status_t solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char **err_msg)
{
    (void)sock; (void)host; (void)port; (void)backlog;
    solo_socket_set_err(err_msg, "not supported without WATT-32");
    return SOLO_SOCK_ERROR;
}

solo_socket_status_t solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char **err_msg)
{
    (void)sock; (void)host; (void)port;
    solo_socket_set_err(err_msg, "not supported without WATT-32");
    return SOLO_SOCK_ERROR;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                  int *client_port, char **err_msg)
{
    (void)server; (void)client_ip; (void)ip_len; (void)client_port;
    solo_socket_set_err(err_msg, "not supported without WATT-32");
    return NULL;
}

solo_socket_status_t solo_socket_backend_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    (void)sock; (void)buf; (void)len;
    if (sent != NULL) *sent = 0;
    return SOLO_SOCK_ERROR;
}

solo_socket_status_t solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len, size_t *received)
{
    (void)sock; (void)buf; (void)max_len;
    if (received != NULL) *received = 0;
    return SOLO_SOCK_ERROR;
}

solo_socket_status_t solo_socket_sendto(solo_socket_t *sock, const char *buf, size_t len,
                                        const char *host, int port, size_t *sent)
{
    (void)sock; (void)buf; (void)len; (void)host; (void)port;
    if (sent != NULL) *sent = 0;
    return SOLO_SOCK_ERROR;
}

solo_socket_status_t solo_socket_recvfrom(solo_socket_t *sock, char *buf, size_t max_len,
                                          char *src_ip, size_t ip_len, int *src_port,
                                          size_t *received)
{
    (void)sock; (void)buf; (void)max_len; (void)src_ip; (void)ip_len; (void)src_port;
    if (received != NULL) *received = 0;
    return SOLO_SOCK_ERROR;
}

solo_socket_status_t solo_socket_backend_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    (void)sock; (void)mode; (void)err_msg;
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_backend_set_timeout(solo_socket_t *sock, int timeout_ms, char **err_msg)
{
    (void)sock; (void)timeout_ms; (void)err_msg;
    return SOLO_SOCK_OK;
}

int solo_socket_backend_poll(solo_pollfd_t *fds, size_t nfds, int timeout_ms)
{
    (void)fds; (void)nfds; (void)timeout_ms;
    return 0;
}

#endif /* USE_WATT32 */

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_dos;
#endif /* SOLO_IS_DOS */
