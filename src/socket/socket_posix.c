
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
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

#if SOLO_IS_POSIX

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

struct solo_socket {
    int fd;
};

solo_socket_t *solo_socket_connect(const char *host, int port, char **err_msg)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *rp = NULL;
    char port_str[16];
    int status;
    int fd = -1;
    solo_socket_t *sock;

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    sprintf(port_str, "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(host, port_str, &hints, &res);
    if (status != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(gai_strerror(status));
        }
        return NULL;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; /* Connected successfully */
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);

    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
        }
        return NULL;
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
    return sock;
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, char **err_msg)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char port_str[16];
    int status;
    int fd;
    int opt = 1;
    solo_socket_t *sock;

    sprintf(port_str, "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
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
        return NULL;
    }

    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
        }
        freeaddrinfo(res);
        return NULL;
    }

    /* Enable SO_REUSEADDR to avoid address in use errors on restart */
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt));

    if (bind(fd, res->ai_addr, res->ai_addrlen) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
        }
        close(fd);
        freeaddrinfo(res);
        return NULL;
    }

    freeaddrinfo(res);

    if (listen(fd, backlog > 0 ? backlog : 5) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
        }
        close(fd);
        return NULL;
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
    return sock;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                        int *client_port, char **err_msg)
{
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    int client_fd;
    solo_socket_t *client_sock;

    if (server == NULL || server->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

    client_fd = accept(server->fd, (struct sockaddr *)&addr, &addr_len);
    if (client_fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
        }
        return NULL;
    }

    if (client_ip != NULL && ip_len > 0) {
        char host_buf[NI_MAXHOST];
        char serv_buf[NI_MAXSERV];
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

    client_sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (client_sock == NULL) {
        close(client_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }
    client_sock->fd = client_fd;
    return client_sock;
}

int solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len)
{
    ssize_t n;
    if (sock == NULL || sock->fd < 0 || buf == NULL || max_len == 0) {
        return -1;
    }

    n = read(sock->fd, buf, max_len);
    if (n < 0) {
        if (errno == EINTR) {
            return solo_socket_read(sock, buf, max_len);
        }
        return -1;
    }
    return (int)n;
}

int solo_socket_write(solo_socket_t *sock, const char *buf, size_t len)
{
    size_t total = 0;
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return -1;
    }

    while (total < len) {
        ssize_t n;
#ifdef MSG_NOSIGNAL
        n = send(sock->fd, buf + total, len - total, MSG_NOSIGNAL);
#else
        n = write(sock->fd, buf + total, len - total);
#endif
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            break;
        }
        total += (size_t)n;
    }

    return (int)total;
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

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_posix;
#endif /* SOLO_IS_POSIX */

#if SOLO_IS_POSIX
#include <fcntl.h>
int solo_socket_set_mode(solo_socket_t *sock, int mode, char **err_msg)
{
    int flags;
    if (sock == NULL || sock->fd < 0) {
        if (err_msg) *err_msg = safe_strdup("Invalid socket handle");
        return -1;
    }
    flags = fcntl(sock->fd, F_GETFL, 0);
    if (flags < 0) {
        if (err_msg) *err_msg = safe_strdup(strerror(errno));
        return -1;
    }
    if (mode == SOLO_SOCK_NONBLOCKING) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    if (fcntl(sock->fd, F_SETFL, flags) < 0) {
        if (err_msg) *err_msg = safe_strdup(strerror(errno));
        return -1;
    }
    return 0;
}
#endif
