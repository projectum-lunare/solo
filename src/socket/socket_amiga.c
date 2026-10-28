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

#if SOLO_IS_AMIGA && !SOLO_IS_POSIX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>

#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>

#if defined(__has_include)
#if __has_include(<proto/bsdsocket.h>)
#include <proto/bsdsocket.h>
#define HAVE_PROTO_BSDSOCKET 1
#endif
#endif

#ifndef HAVE_PROTO_BSDSOCKET
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

struct solo_socket {
    int fd;
};

struct Library *SocketBase = NULL;

#ifndef HAVE_PROTO_BSDSOCKET
extern int CloseSocket(int);
#endif

static char *amiga_inet_ntoa(struct in_addr addr)
{
#if HAVE_PROTO_BSDSOCKET
    return (char *)Inet_NtoA(addr.s_addr);
#else
    return inet_ntoa(addr);
#endif
}

static int socket_init_count = 0;

static int ensure_bsdsocket_open(char **err_msg)
{
    if (socket_init_count == 0) {
        if (SocketBase == NULL) {
            SocketBase = OpenLibrary("bsdsocket.library", 4);
            if (SocketBase == NULL) {
                SocketBase = OpenLibrary("bsdsocket.library", 3);
            }
            if (SocketBase == NULL) {
                if (err_msg != NULL) {
                    *err_msg = safe_strdup(
                        "TCP/IP stack (bsdsocket.library) is not running on this Amiga");
                }
                return 0;
            }
        }
    }
    socket_init_count++;
    return 1;
}

static void ensure_bsdsocket_close(void)
{
    if (socket_init_count > 0) {
        socket_init_count--;
        if (socket_init_count == 0 && SocketBase != NULL) {
            CloseLibrary(SocketBase);
            SocketBase = NULL;
        }
    }
}

solo_socket_t *solo_socket_connect(const char *host, int port, char **err_msg)
{
    struct sockaddr_in sin;
    int fd;
    solo_socket_t *sock;

    if (!ensure_bsdsocket_open(err_msg)) {
        return NULL;
    }

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    sin.sin_addr.s_addr = inet_addr((STRPTR)host);
    if (sin.sin_addr.s_addr == (unsigned long)-1) {
        struct hostent *he = gethostbyname((STRPTR)host);
        if (he == NULL || he->h_addr == NULL) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("cannot resolve host name");
            }
            ensure_bsdsocket_close();
            return NULL;
        }
        memcpy(&sin.sin_addr, he->h_addr, he->h_length);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to create socket");
        }
        ensure_bsdsocket_close();
        return NULL;
    }

    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to connect to host");
        }
        CloseSocket(fd);
        ensure_bsdsocket_close();
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        CloseSocket(fd);
        ensure_bsdsocket_close();
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
    struct sockaddr_in sin;
    int fd;
    int opt = 1;
    solo_socket_t *sock;

    if (!ensure_bsdsocket_open(err_msg)) {
        return NULL;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    if (host == NULL || host[0] == '\0' || strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0) {
        sin.sin_addr.s_addr = INADDR_ANY;
    } else {
        sin.sin_addr.s_addr = inet_addr((STRPTR)host);
        if (sin.sin_addr.s_addr == (unsigned long)-1) {
            struct hostent *he = gethostbyname((STRPTR)host);
            if (he != NULL && he->h_addr != NULL) {
                memcpy(&sin.sin_addr, he->h_addr, he->h_length);
            } else {
                sin.sin_addr.s_addr = INADDR_ANY;
            }
        }
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to create server socket");
        }
        ensure_bsdsocket_close();
        return NULL;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to bind server socket");
        }
        CloseSocket(fd);
        ensure_bsdsocket_close();
        return NULL;
    }

    if (listen(fd, backlog > 0 ? backlog : 5) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to listen on server socket");
        }
        CloseSocket(fd);
        ensure_bsdsocket_close();
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        CloseSocket(fd);
        ensure_bsdsocket_close();
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
    struct sockaddr_in client_addr;
    LONG addr_len = sizeof(client_addr);
    int client_fd;
    solo_socket_t *client_sock;

    if (server == NULL || server->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

    client_fd = accept(server->fd, (struct sockaddr *)&client_addr, (void *)&addr_len);
    if (client_fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("accept failed on bsdsocket");
        }
        return NULL;
    }

    if (client_ip != NULL && ip_len > 0) {
        char *ip_str = amiga_inet_ntoa(client_addr.sin_addr);
        if (ip_str != NULL) {
            strncpy(client_ip, ip_str, ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        } else {
            strncpy(client_ip, "unknown", ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        }
        if (client_port != NULL) {
            *client_port = (int)ntohs(client_addr.sin_port);
        }
    }

    client_sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (client_sock == NULL) {
        CloseSocket(client_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }
    client_sock->fd = client_fd;
    socket_init_count++;
    return client_sock;
}

int solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len)
{
    LONG n;
    if (sock == NULL || sock->fd < 0 || buf == NULL || max_len == 0) {
        return -1;
    }

    n = recv(sock->fd, buf, (LONG)max_len, 0);
    return (int)n;
}

int solo_socket_write(solo_socket_t *sock, const char *buf, size_t len)
{
    size_t total = 0;
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return -1;
    }

    while (total < len) {
        LONG n = send(sock->fd, (char *)(buf + total), (LONG)(len - total), 0);
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }

    return (int)total;
}

void solo_socket_close(solo_socket_t *sock)
{
    if (sock != NULL) {
        if (sock->fd >= 0) {
            CloseSocket(sock->fd);
            sock->fd = -1;
            ensure_bsdsocket_close();
        }
        free(sock);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_amiga;
#endif /* SOLO_IS_AMIGA && !SOLO_IS_POSIX */

int solo_socket_set_mode(solo_socket_t *sock, int mode, char **err_msg)
{
    (void)sock;
    (void)mode;
    (void)err_msg;
    return 0;
}
