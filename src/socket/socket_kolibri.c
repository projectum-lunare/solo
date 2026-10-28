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
#define KOLIBRI_IPPROTO_TCP 6

#pragma pack(push, 1)
struct kolibri_sockaddr_in {
    uint16_t sin_family; /* KOLIBRI_AF_INET4 = 2 */
    uint16_t sin_port;   /* Port in network byte order */
    uint32_t sin_addr;   /* IPv4 in network byte order */
    uint8_t sin_zero[8];
};
#pragma pack(pop)

struct solo_socket {
    int handle;    /* KolibriOS socket handle */
    int is_server; /* Flag if listening server */
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

static int sys_socket_send(int handle, const void *buf, size_t len, int flags)
{
    int ret;
    __asm__ __volatile__("int $0x40"
                         : "=a"(ret)
                         : "a"(KOLIBRI_SYSFN_SOCKET), "b"(KOLIBRI_FN_SOCKET_SEND), "c"(handle),
                           "d"(buf), "S"(len), "D"(flags)
                         : "memory");
    return ret;
}

static int sys_socket_recv(int handle, void *buf, size_t len, int flags)
{
    int ret;
    __asm__ __volatile__("int $0x40"
                         : "=a"(ret)
                         : "a"(KOLIBRI_SYSFN_SOCKET), "b"(KOLIBRI_FN_SOCKET_RECV), "c"(handle),
                           "d"(buf), "S"(len), "D"(flags)
                         : "memory");
    return ret;
}

static int parse_ipv4(const char *s, uint32_t *out_ip)
{
    unsigned int a, b, c, d;
    char tail;

    if (s == NULL || s[0] == '\0' || strcmp(s, "0.0.0.0") == 0 || strcmp(s, "*") == 0) {
        *out_ip = 0; /* INADDR_ANY */
        return 1;
    }
    if (strcmp(s, "localhost") == 0 || strcmp(s, "127.0.0.1") == 0) {
        *out_ip = kolibri_htonl((127U << 24) | 1U);
        return 1;
    }
    if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) == 4) {
        if (a <= 255 && b <= 255 && c <= 255 && d <= 255) {
            *out_ip = kolibri_htonl(((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) |
                                    (uint32_t)d);
            return 1;
        }
    }
    return 0;
}

solo_socket_t *solo_socket_connect(const char *host, int port, char **err_msg)
{
    uint32_t ip;
    struct kolibri_sockaddr_in addr;
    int handle;
    int rc;
    solo_socket_t *sock;

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    if (!parse_ipv4(host, &ip)) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: invalid or unsupported IPv4 host address");
        }
        return NULL;
    }

    handle = sys_socket_open(KOLIBRI_AF_INET4, KOLIBRI_SOCK_STREAM, KOLIBRI_IPPROTO_TCP);
    if (handle < 0) {
        if (err_msg != NULL) {
            *err_msg =
                safe_strdup("KolibriOS: failed to create TCP socket (network stack not loaded?)");
        }
        return NULL;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = KOLIBRI_AF_INET4;
    addr.sin_port = kolibri_htons((uint16_t)port);
    addr.sin_addr = ip;

    rc = sys_socket_connect(handle, &addr, sizeof(addr));
    if (rc != 0) {
        sys_socket_close(handle);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: failed to connect to remote host");
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
    sock->is_server = 0;
    return sock;
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, char **err_msg)
{
    uint32_t ip;
    struct kolibri_sockaddr_in addr;
    int handle;
    int rc;
    solo_socket_t *sock;

    if (!parse_ipv4(host, &ip)) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: invalid bind address");
        }
        return NULL;
    }

    handle = sys_socket_open(KOLIBRI_AF_INET4, KOLIBRI_SOCK_STREAM, KOLIBRI_IPPROTO_TCP);
    if (handle < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: failed to create TCP socket");
        }
        return NULL;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = KOLIBRI_AF_INET4;
    addr.sin_port = kolibri_htons((uint16_t)port);
    addr.sin_addr = ip;

    rc = sys_socket_bind(handle, &addr, sizeof(addr));
    if (rc != 0) {
        sys_socket_close(handle);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: failed to bind socket");
        }
        return NULL;
    }

    rc = sys_socket_listen(handle, backlog > 0 ? backlog : 5);
    if (rc != 0) {
        sys_socket_close(handle);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: failed to listen on socket");
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
    sock->is_server = 1;
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
        if (err_msg != NULL) {
            *err_msg = safe_strdup("KolibriOS: accept failed");
        }
        return NULL;
    }

    if (client_ip != NULL && ip_len > 0) {
        uint32_t ip = kolibri_ntohl(client_addr.sin_addr);
        sprintf(client_ip, "%u.%u.%u.%u", (unsigned int)((ip >> 24) & 0xFF),
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
    sock->is_server = 0;
    return sock;
}

int solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len)
{
    int ret;
    if (sock == NULL || sock->handle < 0 || buf == NULL || max_len == 0) {
        return -1;
    }

    ret = sys_socket_recv(sock->handle, buf, max_len, 0);
    if (ret < 0) {
        return -1;
    }
    return ret;
}

int solo_socket_write(solo_socket_t *sock, const char *buf, size_t len)
{
    int ret;
    if (sock == NULL || sock->handle < 0 || buf == NULL) {
        return -1;
    }

    ret = sys_socket_send(sock->handle, buf, len, 0);
    if (ret < 0) {
        return -1;
    }
    return ret;
}

void solo_socket_close(solo_socket_t *sock)
{
    if (sock == NULL)
        return;
    if (sock->handle >= 0) {
        sys_socket_close(sock->handle);
        sock->handle = -1;
    }
    free(sock);
}

#else
typedef int make_iso_compilers_happy_socket_kolibri;
#endif /* SOLO_IS_KOLIBRI */

int solo_socket_set_mode(solo_socket_t *sock, int mode, char **err_msg)
{
    (void)sock;
    (void)mode;
    (void)err_msg;
    return 0;
}
