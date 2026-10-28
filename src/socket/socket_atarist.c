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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <solo/socket.h>

struct solo_socket {
    int fd;
};

static char *format_socket_error(const char *op)
{
    char buf[128];
    if (errno == ENOSYS || errno == EPROTONOSUPPORT) {
        snprintf(buf, sizeof(buf), "%s: TCP networking requires FreeMiNT MintNet stack", op);
    } else {
        snprintf(buf, sizeof(buf), "%s failed: %s", op, strerror(errno));
    }
    return safe_strdup(buf);
}

solo_socket_t *solo_socket_connect(const char *host, int port, char **err_msg)
{
    int fd;
    struct sockaddr_in sin;
    unsigned long ip;
    solo_socket_t *sock;

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
            memcpy(&sin.sin_addr, he->h_addr_list[0], sizeof(sin.sin_addr));
        } else {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("cannot resolve hostname");
            }
            return NULL;
        }
    } else {
        sin.sin_addr.s_addr = ip;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = format_socket_error("socket()");
        }
        return NULL;
    }

    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        if (err_msg != NULL) {
            *err_msg = format_socket_error("connect()");
        }
        close(fd);
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        close(fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory allocating socket handle");
        }
        return NULL;
    }

    sock->fd = fd;
    return sock;
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, char **err_msg)
{
    int fd;
    struct sockaddr_in sin;
    int opt = 1;
    solo_socket_t *sock;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = format_socket_error("socket()");
        }
        return NULL;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    if (host == NULL || host[0] == '\0' || strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0) {
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        unsigned long ip = inet_addr(host);
        if (ip == (unsigned long)INADDR_NONE && strcmp(host, "255.255.255.255") != 0) {
            struct hostent *he = gethostbyname(host);
            if (he != NULL && he->h_addr_list != NULL && he->h_addr_list[0] != NULL) {
                memcpy(&sin.sin_addr, he->h_addr_list[0], sizeof(sin.sin_addr));
            } else {
                if (err_msg != NULL) {
                    *err_msg = safe_strdup("cannot resolve bind hostname");
                }
                close(fd);
                return NULL;
            }
        } else {
            sin.sin_addr.s_addr = ip;
        }
    }

    if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        if (err_msg != NULL) {
            *err_msg = format_socket_error("bind()");
        }
        close(fd);
        return NULL;
    }

    if (listen(fd, backlog > 0 ? backlog : 5) < 0) {
        if (err_msg != NULL) {
            *err_msg = format_socket_error("listen()");
        }
        close(fd);
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        close(fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory allocating server socket handle");
        }
        return NULL;
    }

    sock->fd = fd;
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
            *err_msg = safe_strdup("invalid server socket handle");
        }
        return NULL;
    }

    memset(&sin, 0, sizeof(sin));
    client_fd = accept(server->fd, (struct sockaddr *)&sin, &addrlen);
    if (client_fd < 0) {
        if (err_msg != NULL) {
            *err_msg = format_socket_error("accept()");
        }
        return NULL;
    }

    if (client_ip != NULL && ip_len > 0) {
        const char *ip_str = inet_ntoa(sin.sin_addr);
        if (ip_str != NULL) {
            strncpy(client_ip, ip_str, ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        }
    }

    if (client_port != NULL) {
        *client_port = ntohs(sin.sin_port);
    }

    client_sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (client_sock == NULL) {
        close(client_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory allocating client socket handle");
        }
        return NULL;
    }

    client_sock->fd = client_fd;
    return client_sock;
}

int solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len)
{
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return -1;
    }
    return (int)read(sock->fd, buf, max_len);
}

int solo_socket_write(solo_socket_t *sock, const char *buf, size_t len)
{
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return -1;
    }
    return (int)write(sock->fd, buf, len);
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
typedef int make_iso_compilers_happy_socket_atarist;
#endif /* SOLO_IS_ATARIST */

int solo_socket_set_mode(solo_socket_t *sock, int mode, char **err_msg)
{
    (void)sock;
    (void)mode;
    (void)err_msg;
    return 0;
}
