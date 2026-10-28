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

#if SOLO_IS_KOLIBRI

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>

/*
 * KolibriOS System Function 75: Network & Sockets API
 * Invoked via software interrupt: int $0x40 with EAX = 75
 */
#define KOLIBRI_SYSFN_SOCKET 75

#define KOLIBRI_FN_SOCKET_OPEN    0
#define KOLIBRI_FN_SOCKET_CLOSE   1
#define KOLIBRI_FN_SOCKET_BIND    2
#define KOLIBRI_FN_SOCKET_LISTEN  3
#define KOLIBRI_FN_SOCKET_CONNECT 4
#define KOLIBRI_FN_SOCKET_ACCEPT  5
#define KOLIBRI_FN_SOCKET_SEND    6
#define KOLIBRI_FN_SOCKET_RECV    7

#define KOLIBRI_AF_INET4    2
#define KOLIBRI_SOCK_STREAM 1
#define KOLIBRI_SOCK_DGRAM  2
#define KOLIBRI_IPPROTO_TCP 6
#define KOLIBRI_IPPROTO_UDP 17

#define KOLIBRI_MSG_DONTWAIT 0x40

#pragma pack(push, 1)
struct kolibri_sockaddr_in {
    uint16_t sin_family; /* KOLIBRI_AF_INET4 = 2 */
    uint16_t sin_port;   /* Port in network byte order */
    uint32_t sin_addr;   /* IPv4 in network byte order */
    uint8_t sin_zero[8];
};
#pragma pack(pop)

struct solo_socket {
    int handle;
    solo_socket_type_t type;
    solo_socket_mode_t mode;
    int timeout_ms;
};

static uint16_t kolibri_htons(uint16_t val)
{
    return (uint16_t)(((val >> 8) & 0x00FF) | ((val << 8) & 0xFF00));
}

static uint16_t kolibri_ntohs(uint16_t val)
{
    return kolibri_htons(val);
}

static uint32_t kolibri_htonl(uint32_t val)
{
    return ((val >> 24) & 0x000000FF) | ((val >> 8) & 0x0000FF00) | ((val << 8) & 0x00FF0000) |
           ((val << 24) & 0xFF000000);
}

static uint32_t kolibri_ntohl(uint32_t val)
{
    return kolibri_htonl(val);
}

/*
 * KolibriOS int 0x40 syscall wrappers for sysfn 75
 */
static int sys_socket_open(int domain, int type, int proto)
{
    int ret;
    __asm__ __volatile__("int $0x40"
                         : "=a"(ret)
                         : "a"(KOLIBRI_SYSFN_SOCKET), "b"(KOLIBRI_FN_SOCKET_OPEN), "c"(domain),
                           "d"(type), "S"(proto)
                         : "memory");
    return ret;
}

static int sys_socket_close(int handle)
{
    int ret;
    __asm__ __volatile__("int $0x40"
                         : "=a"(ret)
                         : "a"(KOLIBRI_SYSFN_SOCKET), "b"(KOLIBRI_FN_SOCKET_CLOSE), "c"(handle)
                         : "memory");
    return ret;
}

static int sys_socket_bind(int handle, const void *addr, int addrlen)
{
    int ret;
    __asm__ __volatile__("int $0x40"
                         : "=a"(ret)
                         : "a"(KOLIBRI_SYSFN_SOCKET), "b"(KOLIBRI_FN_SOCKET_BIND), "c"(handle),
                           "d"(addr), "S"(addrlen)
                         : "memory");
    return ret;
}

static int sys_socket_listen(int handle, int backlog)
{
    int ret;
    __asm__ __volatile__("int $0x40"
                         : "=a"(ret)
                         : "a"(KOLIBRI_SYSFN_SOCKET), "b"(KOLIBRI_FN_SOCKET_LISTEN), "c"(handle),
                           "d"(backlog)
                         : "memory");
    return ret;
}

static int sys_socket_connect(int handle, const void *addr, int addrlen)
{
    int ret;
    __asm__ __volatile__("int $0x40"
                         : "=a"(ret)
                         : "a"(KOLIBRI_SYSFN_SOCKET), "b"(KOLIBRI_FN_SOCKET_CONNECT), "c"(handle),
                           "d"(addr), "S"(addrlen)
                         : "memory");
    return ret;
}

static int sys_socket_accept(int handle, void *addr, int *addrlen)
{
    int ret;
    __asm__ __volatile__("int $0x40"
                         : "=a"(ret)
                         : "a"(KOLIBRI_SYSFN_SOCKET), "b"(KOLIBRI_FN_SOCKET_ACCEPT), "c"(handle),
                           "d"(addr), "S"(addrlen)
                         : "memory");
    return ret;
}

static int sys_socket_send(int handle, const void *buf, int len, int flags)
{
    int ret;
    __asm__ __volatile__("int $0x40"
                         : "=a"(ret)
                         : "a"(KOLIBRI_SYSFN_SOCKET), "b"(KOLIBRI_FN_SOCKET_SEND), "c"(handle),
                           "d"(buf), "S"(len), "D"(flags)
                         : "memory");
    return ret;
}

static int sys_socket_recv(int handle, void *buf, int len, int flags)
{
    int ret;
    __asm__ __volatile__("int $0x40"
                         : "=a"(ret)
                         : "a"(KOLIBRI_SYSFN_SOCKET), "b"(KOLIBRI_FN_SOCKET_RECV), "c"(handle),
                           "d"(buf), "S"(len), "D"(flags)
                         : "memory");
    return ret;
}

/* KolibriOS Syscall 5: sleep for given hundredths of a second */
static void sys_sleep_ticks(int ticks)
{
    if (ticks <= 0) ticks = 1;
    __asm__ __volatile__("int $0x40" :: "a"(5), "b"(ticks) : "memory");
}

static int parse_ipv4(const char *host, uint32_t *out_ip)
{
    unsigned int a, b, c, d;
    char tail[2];

    if (host == NULL || host[0] == '\0' || strcmp(host, "127.0.0.1") == 0) {
        *out_ip = kolibri_htonl(0x7F000001);
        return 1;
    }

    if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0) {
        *out_ip = 0;
        return 1;
    }

    if (sscanf(host, "%u.%u.%u.%u%1s", &a, &b, &c, &d, tail) == 4) {
        if (a <= 255 && b <= 255 && c <= 255 && d <= 255) {
            uint32_t host_order = (a << 24) | (b << 16) | (c << 8) | d;
            *out_ip = kolibri_htonl(host_order);
            return 1;
        }
    }

    return 0;
}

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char **err_msg)
{
    int stype = (type == SOLO_SOCK_UDP) ? KOLIBRI_SOCK_DGRAM : KOLIBRI_SOCK_STREAM;
    int proto = (type == SOLO_SOCK_UDP) ? KOLIBRI_IPPROTO_UDP : KOLIBRI_IPPROTO_TCP;
    int handle;
    solo_socket_t *sock;

    handle = sys_socket_open(KOLIBRI_AF_INET4, stype, proto);
    if (handle < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: failed to create socket (network stack running?)");
        }
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        sys_socket_close(handle);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }

    sock->handle = handle;
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
        if (sock->handle >= 0) {
            sys_socket_close(sock->handle);
            sock->handle = -1;
        }
        free(sock);
    }
}

int solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char **err_msg)
{
    uint32_t ip;
    struct kolibri_sockaddr_in addr;
    int rc;

    if (sock == NULL || sock->handle < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    if (!parse_ipv4(host, &ip)) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: invalid bind address");
        }
        return SOLO_SOCK_ERROR;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = KOLIBRI_AF_INET4;
    addr.sin_port = kolibri_htons((uint16_t)port);
    addr.sin_addr = ip;

    rc = sys_socket_bind(sock->handle, &addr, sizeof(addr));
    if (rc != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: failed to bind socket");
        }
        return SOLO_SOCK_ERROR;
    }

    if (sock->type == SOLO_SOCK_TCP) {
        rc = sys_socket_listen(sock->handle, backlog > 0 ? backlog : 5);
        if (rc != 0) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("KolibriOS: failed to listen on socket");
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

int solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char **err_msg)
{
    uint32_t ip;
    struct kolibri_sockaddr_in addr;
    int rc;

    if (sock == NULL || sock->handle < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    if (!parse_ipv4(host, &ip)) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: invalid IPv4 address format");
        }
        return SOLO_SOCK_ERROR;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = KOLIBRI_AF_INET4;
    addr.sin_port = kolibri_htons((uint16_t)port);
    addr.sin_addr = ip;

    rc = sys_socket_connect(sock->handle, &addr, sizeof(addr));
    if (rc != 0) {
        if (sock->mode == SOLO_SOCK_NONBLOCKING && (rc == -11 || rc == -1)) {
            return SOLO_SOCK_AGAIN;
        }
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: failed to connect to remote host");
        }
        return SOLO_SOCK_ERROR;
    }

    return SOLO_SOCK_OK;
}

solo_socket_t *solo_socket_connect(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    solo_socket_t *sock = solo_socket_create(SOLO_SOCK_TCP, mode, err_msg);
    int status;
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
    struct kolibri_sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    int client_handle;
    solo_socket_t *sock;

    if (server == NULL || server->handle < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

    memset(&client_addr, 0, sizeof(client_addr));
    client_handle = sys_socket_accept(server->handle, &client_addr, &addr_len);
    if (client_handle < 0) {
        if (server->mode == SOLO_SOCK_NONBLOCKING && (client_handle == -11 || client_handle == -1)) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("resource temporarily unavailable");
            }
            return NULL;
        }
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: accept failed");
        }
        return NULL;
    }

    if (client_ip != NULL && ip_len > 0) {
        uint32_t ip = kolibri_ntohl(client_addr.sin_addr);
        snprintf(client_ip, ip_len, "%u.%u.%u.%u", (unsigned int)((ip >> 24) & 0xFF),
                 (unsigned int)((ip >> 16) & 0xFF), (unsigned int)((ip >> 8) & 0xFF),
                 (unsigned int)(ip & 0xFF));
    }

    if (client_port != NULL) {
        *client_port = (int)kolibri_ntohs(client_addr.sin_port);
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        sys_socket_close(client_handle);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }

    sock->handle = client_handle;
    sock->type = SOLO_SOCK_TCP;
    sock->mode = server->mode;
    sock->timeout_ms = server->timeout_ms;
    return sock;
}

int solo_socket_send(solo_socket_t *sock, const char *buf, size_t len)
{
    int flags = 0;
    int ret;
    if (sock == NULL || sock->handle < 0 || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }

    if (sock->mode == SOLO_SOCK_NONBLOCKING) {
        flags = KOLIBRI_MSG_DONTWAIT;
    }

    ret = sys_socket_send(sock->handle, buf, (int)len, flags);
    if (ret < 0) {
        if (ret == -11 || (sock->mode == SOLO_SOCK_NONBLOCKING && ret == -1)) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    return ret;
}

int solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len)
{
    int flags = 0;
    int ret;
    if (sock == NULL || sock->handle < 0 || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }

    if (sock->mode == SOLO_SOCK_NONBLOCKING) {
        flags = KOLIBRI_MSG_DONTWAIT;
    }

    ret = sys_socket_recv(sock->handle, buf, (int)max_len, flags);
    if (ret < 0) {
        if (ret == -11 || (sock->mode == SOLO_SOCK_NONBLOCKING && ret == -1)) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    if (ret == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
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
    uint32_t ip;
    struct kolibri_sockaddr_in addr;

    if (sock == NULL || sock->handle < 0 || buf == NULL || host == NULL) {
        return SOLO_SOCK_ERROR;
    }

    if (!parse_ipv4(host, &ip)) {
        return SOLO_SOCK_ERROR;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = KOLIBRI_AF_INET4;
    addr.sin_port = kolibri_htons((uint16_t)port);
    addr.sin_addr = ip;

    sys_socket_connect(sock->handle, &addr, sizeof(addr));
    return solo_socket_send(sock, buf, len);
}

int solo_socket_recvfrom(solo_socket_t *sock, char *buf, size_t max_len,
                         char *src_ip, size_t ip_len, int *src_port)
{
    int n = solo_socket_recv(sock, buf, max_len);
    if (n > 0) {
        if (src_ip != NULL && ip_len > 0) {
            src_ip[0] = '\0';
        }
        if (src_port != NULL) {
            *src_port = 0;
        }
    }
    return n;
}

int solo_socket_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    if (sock == NULL || sock->handle < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
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
    if (sock == NULL || sock->handle < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid socket handle");
        }
        return SOLO_SOCK_ERROR;
    }

    sock->timeout_ms = timeout_ms;
    return SOLO_SOCK_OK;
}

int solo_socket_wait(solo_socket_t *sock, int events, int timeout_ms)
{
    int elapsed = 0;
    int step_ms = 10;

    if (sock == NULL || sock->handle < 0) {
        return SOLO_SOCK_ERROR;
    }

    while (timeout_ms < 0 || elapsed <= timeout_ms) {
        /*
         * KolibriOS sysfn 75 wait: check if socket is ready
         */
        int ret = 0;
        if (events & SOLO_SOCK_POLL_READ) {
            char peek;
            int n = sys_socket_recv(sock->handle, &peek, 1, KOLIBRI_MSG_DONTWAIT);
            if (n >= 0) {
                ret |= SOLO_SOCK_POLL_READ;
            }
        }
        if (events & SOLO_SOCK_POLL_WRITE) {
            ret |= SOLO_SOCK_POLL_WRITE;
        }

        if (ret != 0) {
            return ret;
        }

        if (timeout_ms >= 0 && elapsed >= timeout_ms) {
            break;
        }

        sys_sleep_ticks(1); /* 10 ms sleep */
        elapsed += step_ms;
    }

    return 0;
}

int solo_socket_get_fd(const solo_socket_t *sock)
{
    return sock ? sock->handle : -1;
}

#else
typedef int make_iso_compilers_happy_socket_kolibri;
#endif /* SOLO_IS_KOLIBRI */
