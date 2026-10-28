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
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

typedef unsigned long socklen_t;

extern int CloseSocket(int);
extern int IoctlSocket(int, unsigned long, char *);
extern int WaitSelect(int, void *, void *, void *, struct timeval *, unsigned long *);
extern int Errno(void);
#endif

#ifndef FIONBIO
#define FIONBIO 0x8004667e
#endif

struct solo_socket {
    int fd;
    solo_socket_type_t type;
    solo_socket_mode_t mode;
    int timeout_ms;
};

struct Library *SocketBase = NULL;

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

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char **err_msg)
{
    int socktype = (type == SOLO_SOCK_UDP) ? SOCK_DGRAM : SOCK_STREAM;
    int fd;
    solo_socket_t *sock;

    if (!ensure_bsdsocket_open(err_msg)) {
        return NULL;
    }

    fd = socket(AF_INET, socktype, 0);
    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to create socket");
        }
        ensure_bsdsocket_close();
        return NULL;
    }

    if (mode == SOLO_SOCK_NONBLOCKING) {
        char nb = 1;
        IoctlSocket(fd, FIONBIO, &nb);
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
            CloseSocket(sock->fd);
            sock->fd = -1;
            ensure_bsdsocket_close();
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

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    if (host == NULL || strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0 || host[0] == '\0') {
        sin.sin_addr.s_addr = INADDR_ANY;
    } else {
        sin.sin_addr.s_addr = inet_addr((STRPTR)(void *)host);
    }

    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

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

solo_socket_status_t solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char **err_msg)
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
    sin.sin_addr.s_addr = inet_addr((STRPTR)(void *)host);

    if (sin.sin_addr.s_addr == INADDR_NONE) {
        struct hostent *he = gethostbyname((STRPTR)(void *)host);
        if (he == NULL || he->h_addr == NULL) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("cannot resolve hostname");
            }
            return SOLO_SOCK_ERROR;
        }
        memcpy(&sin.sin_addr, he->h_addr, he->h_length);
    }

    if (connect(sock->fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
#if HAVE_PROTO_BSDSOCKET
        int err = Errno();
#else
        int err = errno;
#endif
        if (err == EINPROGRESS || err == EWOULDBLOCK) {
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
    socklen_t addr_len = (socklen_t)sizeof(sin);
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
#if HAVE_PROTO_BSDSOCKET
        int err = Errno();
#else
        int err = errno;
#endif
        if (err == EWOULDBLOCK || err == EAGAIN) {
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
        char *ip = amiga_inet_ntoa(sin.sin_addr);
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
        CloseSocket(client_fd);
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
        char nb = 1;
        IoctlSocket(client_fd, FIONBIO, &nb);
    }

    ensure_bsdsocket_open(NULL);
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

    ret = send(sock->fd, (char *)buf, len, 0);
    if (ret < 0) {
#if HAVE_PROTO_BSDSOCKET
        int err = Errno();
#else
        int err = errno;
#endif
        if (err == EWOULDBLOCK || err == EAGAIN) {
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
#if HAVE_PROTO_BSDSOCKET
        int err = Errno();
#else
        int err = errno;
#endif
        if (err == EWOULDBLOCK || err == EAGAIN) {
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
    int ret;

    if (sent != NULL) {
        *sent = 0;
    }
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);
    sin.sin_addr.s_addr = inet_addr((STRPTR)(void *)host);

    ret = sendto(sock->fd, (char *)buf, len, 0, (struct sockaddr *)&sin, sizeof(sin));
    if (ret < 0) {
#if HAVE_PROTO_BSDSOCKET
        int err = Errno();
#else
        int err = errno;
#endif
        if (err == EWOULDBLOCK || err == EAGAIN) {
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
    socklen_t addr_len = (socklen_t)sizeof(sin);
    int n;

    if (received != NULL) {
        *received = 0;
    }
    if (sock == NULL || sock->fd < 0 || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    n = recvfrom(sock->fd, buf, max_len, 0, (struct sockaddr *)&sin, &addr_len);
    if (n < 0) {
#if HAVE_PROTO_BSDSOCKET
        int err = Errno();
#else
        int err = errno;
#endif
        if (err == EWOULDBLOCK || err == EAGAIN) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    if (n == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
    }

    if (src_ip != NULL && ip_len > 0) {
        char *ip = amiga_inet_ntoa(sin.sin_addr);
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

    if (received != NULL) {
        *received = (size_t)n;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    char nb;
    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    nb = (mode == SOLO_SOCK_NONBLOCKING) ? 1 : 0;
    if (IoctlSocket(sock->fd, FIONBIO, &nb) < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("IoctlSocket FIONBIO failed");
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

    ret = WaitSelect(sock->fd + 1, &read_fds, &write_fds, &err_fds, ptv, NULL);
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

int solo_socket_get_fd(const solo_socket_t *sock)
{
    return sock ? sock->fd : -1;
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_amiga;
#endif /* SOLO_IS_AMIGA && !SOLO_IS_POSIX */
