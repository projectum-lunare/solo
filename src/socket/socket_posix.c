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
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
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
    solo_socket_type_t type;
    solo_socket_mode_t mode;
    int timeout_ms;
};

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char **err_msg)
{
    int socktype = (type == SOLO_SOCK_UDP) ? SOCK_DGRAM : SOCK_STREAM;
    int fd = socket(AF_INET, socktype, 0);
    solo_socket_t *sock;

    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
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

int solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char **err_msg)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char port_str[16];
    int status;
    int opt = 1;

    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

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

    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt));

    if (bind(sock->fd, res->ai_addr, res->ai_addrlen) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
        }
        freeaddrinfo(res);
        return SOLO_SOCK_ERROR;
    }

    freeaddrinfo(res);

    if (sock->type == SOLO_SOCK_TCP) {
        if (listen(sock->fd, backlog > 0 ? backlog : 128) != 0) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup(strerror(errno));
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
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *rp = NULL;
    char port_str[16];
    int status;
    int connected = 0;

    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

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
        if (connect(sock->fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            connected = 1;
            break;
        }
        if (errno == EINPROGRESS) {
            freeaddrinfo(res);
            return SOLO_SOCK_AGAIN;
        }
    }

    freeaddrinfo(res);

    if (!connected) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
        }
        return SOLO_SOCK_ERROR;
    }

    return SOLO_SOCK_OK;
}

solo_socket_t *solo_socket_connect(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    solo_socket_t *sock = solo_socket_tcp(mode, err_msg);
    if (sock == NULL) {
        return NULL;
    }

    int res = solo_socket_connect_sock(sock, host, port, err_msg);
    if (res != SOLO_SOCK_OK && res != SOLO_SOCK_AGAIN) {
        solo_socket_close(sock);
        return NULL;
    }

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
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (err_msg != NULL) {
                *err_msg = NULL;
            }
            return NULL;
        }
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
    client_sock->type = SOLO_SOCK_TCP;
    client_sock->mode = server->mode;
    client_sock->timeout_ms = 0;

    if (server->mode == SOLO_SOCK_NONBLOCKING) {
        int flags = fcntl(client_fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    return client_sock;
}

int solo_socket_send(solo_socket_t *sock, const char *buf, size_t len)
{
    size_t total = 0;
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

    if (sock->mode == SOLO_SOCK_NONBLOCKING) {
        ssize_t n;
#ifdef MSG_NOSIGNAL
        n = send(sock->fd, buf, len, MSG_NOSIGNAL);
#else
        n = write(sock->fd, buf, len);
#endif
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return SOLO_SOCK_AGAIN;
            }
            return SOLO_SOCK_ERROR;
        }
        return (int)n;
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
            return SOLO_SOCK_ERROR;
        }
        if (n == 0) {
            break;
        }
        total += (size_t)n;
    }

    return (int)total;
}

int solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len)
{
    ssize_t n;
    if (sock == NULL || sock->fd < 0 || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    while (1) {
        n = read(sock->fd, buf, max_len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return SOLO_SOCK_AGAIN;
            }
            return SOLO_SOCK_ERROR;
        }
        break;
    }

    return (int)n;
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
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    char port_str[16];
    int status;
    ssize_t n;

    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

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

    while (1) {
        n = sendto(sock->fd, buf, len, 0, res->ai_addr, res->ai_addrlen);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                freeaddrinfo(res);
                return SOLO_SOCK_AGAIN;
            }
            freeaddrinfo(res);
            return SOLO_SOCK_ERROR;
        }
        break;
    }

    freeaddrinfo(res);
    return (int)n;
}

int solo_socket_recvfrom(solo_socket_t *sock, char *buf, size_t max_len,
                         char *src_ip, size_t ip_len, int *src_port)
{
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);
    ssize_t n;

    if (sock == NULL || sock->fd < 0 || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    while (1) {
        n = recvfrom(sock->fd, buf, max_len, 0, (struct sockaddr *)&addr, &addr_len);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return SOLO_SOCK_AGAIN;
            }
            return SOLO_SOCK_ERROR;
        }
        break;
    }

    if (src_ip != NULL && ip_len > 0) {
        char host_buf[NI_MAXHOST];
        char serv_buf[NI_MAXSERV];
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

    return (int)n;
}

int solo_socket_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    int flags;
    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    flags = fcntl(sock->fd, F_GETFL, 0);
    if (flags < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
        }
        return SOLO_SOCK_ERROR;
    }

    if (mode == SOLO_SOCK_NONBLOCKING) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(sock->fd, F_SETFL, flags) < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
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
    struct timeval tv;
    if (sock == NULL || sock->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    if (timeout_ms < 0) {
        timeout_ms = 0;
    }

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    if (setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
        }
        return SOLO_SOCK_ERROR;
    }

    if (setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(strerror(errno));
        }
        return SOLO_SOCK_ERROR;
    }

    sock->timeout_ms = timeout_ms;
    return SOLO_SOCK_OK;
}

int solo_socket_wait(solo_socket_t *sock, int events, int timeout_ms)
{
    struct pollfd pfd;
    int ret;
    int ready_events = 0;

    if (sock == NULL || sock->fd < 0) {
        return SOLO_SOCK_ERROR;
    }

    pfd.fd = sock->fd;
    pfd.events = 0;
    pfd.revents = 0;

    if (events & SOLO_SOCK_POLL_READ) {
        pfd.events |= POLLIN;
    }
    if (events & SOLO_SOCK_POLL_WRITE) {
        pfd.events |= POLLOUT;
    }

    while (1) {
        ret = poll(&pfd, 1, timeout_ms);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            return SOLO_SOCK_ERROR;
        }
        break;
    }

    if (ret == 0) {
        return 0; /* Timeout */
    }

    if (pfd.revents & (POLLIN | POLLHUP)) {
        ready_events |= SOLO_SOCK_POLL_READ;
    }
    if (pfd.revents & POLLOUT) {
        ready_events |= SOLO_SOCK_POLL_WRITE;
    }
    if (pfd.revents & (POLLERR | POLLNVAL)) {
        ready_events |= SOLO_SOCK_POLL_ERR;
    }

    return ready_events;
}

int solo_socket_get_fd(const solo_socket_t *sock)
{
    return sock ? sock->fd : -1;
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_posix;
#endif /* SOLO_IS_POSIX */
