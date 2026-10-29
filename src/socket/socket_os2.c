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

#if SOLO_IS_OS2

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>
#include "socket_internal.h"

/*
 * Standalone BSD socket definitions for OS/2 32-bit.
 * Declared locally without including OS/2 TCP/IP headers to prevent
 * Open Watcom from auto-inserting '#pragma library("tcpip32.lib")',
 * ensuring zero static import dependency in the LX linear executable header.
 */
struct os2_in_addr {
    unsigned long s_addr;
};

struct os2_sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    struct os2_in_addr sin_addr;
    char sin_zero[8];
};

struct os2_hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

struct os2_timeval {
    long tv_sec;
    long tv_usec;
};

#define OS2_AF_INET         2
#define OS2_SOCK_STREAM     1
#define OS2_SOCK_DGRAM      2
#define OS2_SOL_SOCKET      0xFFFF
#define OS2_SO_REUSEADDR    0x0004
#define OS2_SO_RCVTIMEO     0x1006
#define OS2_SO_SNDTIMEO     0x1007
#define OS2_INADDR_ANY      0x00000000UL
#define OS2_INADDR_NONE     0xFFFFFFFFUL
#define OS2_FIONBIO         0x8004667eUL

#define OS2_SOCEWOULDBLOCK  35
#define OS2_SOCEINPROGRESS  36
#define OS2_SOCEAGAIN       35

#define OS2_HTONS(x) ((((unsigned short)(x) & 0xFF) << 8) | (((unsigned short)(x) >> 8) & 0xFF))
#define OS2_NTOHS(x) OS2_HTONS(x)
#define OS2_HTONL(x) ((((unsigned long)(x) & 0x000000FFUL) << 24) | \
                      (((unsigned long)(x) & 0x0000FF00UL) << 8)  | \
                      (((unsigned long)(x) & 0x00FF0000UL) >> 8)  | \
                      (((unsigned long)(x) & 0xFF000000UL) >> 24))

/* OS/2 32-bit TCP/IP functions use the _System calling convention */
typedef int(_System *pfn_sock_init_t)(void);
typedef int(_System *pfn_sock_errno_t)(void);
typedef int(_System *pfn_socket_t)(int domain, int type, int protocol);
typedef int(_System *pfn_connect_t)(int s, const void *name, int namelen);
typedef int(_System *pfn_bind_t)(int s, const void *name, int namelen);
typedef int(_System *pfn_listen_t)(int s, int backlog);
typedef int(_System *pfn_accept_t)(int s, void *addr, int *addrlen);
typedef int(_System *pfn_send_t)(int s, const void *msg, size_t len, int flags);
typedef int(_System *pfn_recv_t)(int s, void *buf, size_t len, int flags);
typedef int(_System *pfn_sendto_t)(int s, const void *msg, size_t len, int flags, const void *to, int tolen);
typedef int(_System *pfn_recvfrom_t)(int s, void *buf, size_t len, int flags, void *from, int *fromlen);
typedef int(_System *pfn_soclose_t)(int s);
typedef int(_System *pfn_setsockopt_t)(int s, int level, int optname, const void *optval, int optlen);
typedef struct os2_hostent *(_System *pfn_gethostbyname_t)(const char *name);
typedef int(_System *pfn_os2_ioctl_t)(int s, unsigned long cmd, char *data, int len);
typedef int(_System *pfn_ioctl_t)(int s, unsigned long cmd, char *data);
typedef int(_System *pfn_os2_select_t)(int *s, int reads, int writes, int excepts, long timeout);

static struct {
    HMODULE hmod;
    int initialized;
    int failed;
    pfn_sock_init_t sock_init;
    pfn_sock_errno_t sock_errno;
    pfn_socket_t socket;
    pfn_connect_t connect;
    pfn_bind_t bind;
    pfn_listen_t listen;
    pfn_accept_t accept;
    pfn_send_t send;
    pfn_recv_t recv;
    pfn_sendto_t sendto;
    pfn_recvfrom_t recvfrom;
    pfn_soclose_t soclose;
    pfn_setsockopt_t setsockopt;
    pfn_gethostbyname_t gethostbyname;
    pfn_os2_ioctl_t os2_ioctl;
    pfn_ioctl_t ioctl;
    pfn_os2_select_t os2_select;
} g_tcpip;

static int os2_tcpip_init(char **err_msg)
{
    char errbuf[128];
    APIRET rc;

    if (g_tcpip.initialized) {
        return 0;
    }
    if (g_tcpip.failed) {
        solo_socket_set_err(err_msg, "OS/2 TCP/IP stack (SO32DLL/TCPIP32) is not installed or failed to load");
        return -1;
    }

    rc = DosLoadModule(errbuf, sizeof(errbuf), "SO32DLL", &g_tcpip.hmod);
    if (rc != 0) {
        rc = DosLoadModule(errbuf, sizeof(errbuf), "TCPIP32", &g_tcpip.hmod);
    }

    if (rc != 0) {
        g_tcpip.failed = 1;
        solo_socket_set_err(err_msg, "OS/2 TCP/IP stack (SO32DLL/TCPIP32) is not installed");
        return -1;
    }

    DosQueryProcAddr(g_tcpip.hmod, 0, "sock_init", (PFN *)&g_tcpip.sock_init);
    DosQueryProcAddr(g_tcpip.hmod, 0, "sock_errno", (PFN *)&g_tcpip.sock_errno);
    DosQueryProcAddr(g_tcpip.hmod, 0, "socket", (PFN *)&g_tcpip.socket);
    DosQueryProcAddr(g_tcpip.hmod, 0, "connect", (PFN *)&g_tcpip.connect);
    DosQueryProcAddr(g_tcpip.hmod, 0, "bind", (PFN *)&g_tcpip.bind);
    DosQueryProcAddr(g_tcpip.hmod, 0, "listen", (PFN *)&g_tcpip.listen);
    DosQueryProcAddr(g_tcpip.hmod, 0, "accept", (PFN *)&g_tcpip.accept);
    DosQueryProcAddr(g_tcpip.hmod, 0, "send", (PFN *)&g_tcpip.send);
    DosQueryProcAddr(g_tcpip.hmod, 0, "recv", (PFN *)&g_tcpip.recv);
    DosQueryProcAddr(g_tcpip.hmod, 0, "sendto", (PFN *)&g_tcpip.sendto);
    DosQueryProcAddr(g_tcpip.hmod, 0, "recvfrom", (PFN *)&g_tcpip.recvfrom);
    DosQueryProcAddr(g_tcpip.hmod, 0, "soclose", (PFN *)&g_tcpip.soclose);
    DosQueryProcAddr(g_tcpip.hmod, 0, "setsockopt", (PFN *)&g_tcpip.setsockopt);
    DosQueryProcAddr(g_tcpip.hmod, 0, "gethostbyname", (PFN *)&g_tcpip.gethostbyname);
    DosQueryProcAddr(g_tcpip.hmod, 0, "os2_ioctl", (PFN *)&g_tcpip.os2_ioctl);
    DosQueryProcAddr(g_tcpip.hmod, 0, "ioctl", (PFN *)&g_tcpip.ioctl);
    DosQueryProcAddr(g_tcpip.hmod, 0, "os2_select", (PFN *)&g_tcpip.os2_select);

    if (g_tcpip.sock_init != NULL) {
        g_tcpip.sock_init();
    }

    g_tcpip.initialized = 1;
    return 0;
}

static int os2_resolve_host(const char *host, unsigned long *out_ip, char **err_msg)
{
    unsigned long ip_host;
    host = solo_socket_normalize_connect_host(host);

    if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0) {
        *out_ip = OS2_INADDR_ANY;
        return 0;
    }

    if (solo_socket_parse_ipv4(host, &ip_host)) {
        *out_ip = OS2_HTONL(ip_host);
        return 0;
    }

    if (g_tcpip.gethostbyname != NULL) {
        struct os2_hostent *he = g_tcpip.gethostbyname(host);
        if (he != NULL && he->h_addr_list != NULL && he->h_addr_list[0] != NULL) {
            memcpy(out_ip, he->h_addr_list[0], sizeof(unsigned long));
            return 0;
        }
    }

    solo_socket_set_err(err_msg, "cannot resolve hostname");
    return -1;
}

static int os2_apply_nb(int fd, solo_socket_mode_t mode)
{
    int nb = (mode == SOLO_SOCK_NONBLOCKING) ? 1 : 0;
    if (g_tcpip.os2_ioctl != NULL) {
        return g_tcpip.os2_ioctl(fd, OS2_FIONBIO, (char *)&nb, sizeof(nb));
    }
    if (g_tcpip.ioctl != NULL) {
        return g_tcpip.ioctl(fd, OS2_FIONBIO, (char *)&nb);
    }
    return 0;
}

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char **err_msg)
{
    int stype;
    int fd;
    solo_socket_t *sock;

    if (os2_tcpip_init(err_msg) != 0) {
        return NULL;
    }
    if (g_tcpip.socket == NULL) {
        solo_socket_set_err(err_msg, "socket() not available in OS/2 TCP/IP stack");
        return NULL;
    }

    stype = (type == SOLO_SOCK_UDP) ? OS2_SOCK_DGRAM : OS2_SOCK_STREAM;
    fd = g_tcpip.socket(OS2_AF_INET, stype, 0);
    if (fd < 0) {
        if (err_msg != NULL) {
            int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
            solo_socket_set_err_fmt(err_msg, "socket() failed (error %d)", err);
        }
        return NULL;
    }

    if (mode == SOLO_SOCK_NONBLOCKING) {
        os2_apply_nb(fd, mode);
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        if (g_tcpip.soclose != NULL) {
            g_tcpip.soclose(fd);
        }
        solo_socket_set_err(err_msg, "out of memory");
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
        if (g_tcpip.soclose != NULL) {
            g_tcpip.soclose(sock->fd);
        }
        sock->fd = -1;
    }
}

solo_socket_status_t solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char **err_msg)
{
    struct os2_sockaddr_in sin;
    unsigned long ip;
    int opt = 1;

    if (!SOLO_SOCK_IS_VALID(sock)) {
        solo_socket_set_err(err_msg, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }

    if (os2_resolve_host(solo_socket_normalize_bind_host(host), &ip, err_msg) != 0) {
        return SOLO_SOCK_ERROR;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = OS2_AF_INET;
    sin.sin_port = OS2_HTONS((unsigned short)port);
    sin.sin_addr.s_addr = ip;

    if (g_tcpip.setsockopt != NULL) {
        g_tcpip.setsockopt(sock->fd, OS2_SOL_SOCKET, OS2_SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    }

    if (g_tcpip.bind == NULL || g_tcpip.bind(sock->fd, &sin, sizeof(sin)) < 0) {
        if (err_msg != NULL) {
            int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
            solo_socket_set_err_fmt(err_msg, "bind() failed (error %d)", err);
        }
        return SOLO_SOCK_ERROR;
    }

    if (sock->type == SOLO_SOCK_TCP) {
        if (g_tcpip.listen == NULL || g_tcpip.listen(sock->fd, backlog > 0 ? backlog : 5) < 0) {
            if (err_msg != NULL) {
                int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
                solo_socket_set_err_fmt(err_msg, "listen() failed (error %d)", err);
            }
            return SOLO_SOCK_ERROR;
        }
    }

    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char **err_msg)
{
    struct os2_sockaddr_in sin;
    unsigned long ip;

    if (!SOLO_SOCK_IS_VALID(sock)) {
        solo_socket_set_err(err_msg, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }

    if (os2_resolve_host(host, &ip, err_msg) != 0) {
        return SOLO_SOCK_ERROR;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = OS2_AF_INET;
    sin.sin_port = OS2_HTONS((unsigned short)port);
    sin.sin_addr.s_addr = ip;

    if (g_tcpip.connect == NULL || g_tcpip.connect(sock->fd, &sin, sizeof(sin)) < 0) {
        int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
        if (sock->mode == SOLO_SOCK_NONBLOCKING &&
            (err == OS2_SOCEINPROGRESS || err == OS2_SOCEWOULDBLOCK || err == OS2_SOCEAGAIN)) {
            return SOLO_SOCK_AGAIN;
        }
        if (err_msg != NULL) {
            solo_socket_set_err_fmt(err_msg, "connect() failed (error %d)", err);
        }
        return SOLO_SOCK_ERROR;
    }

    return SOLO_SOCK_OK;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                  int *client_port, char **err_msg)
{
    struct os2_sockaddr_in sin;
    int addrlen = sizeof(sin);
    int client_fd;
    solo_socket_t *client_sock;

    if (!SOLO_SOCK_IS_VALID(server)) {
        solo_socket_set_err(err_msg, "invalid server socket");
        return NULL;
    }

    if (g_tcpip.accept == NULL) {
        solo_socket_set_err(err_msg, "accept() not supported by OS/2 TCP/IP stack");
        return NULL;
    }

    memset(&sin, 0, sizeof(sin));
    client_fd = g_tcpip.accept(server->fd, &sin, &addrlen);
    if (client_fd < 0) {
        int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
        if (server->mode == SOLO_SOCK_NONBLOCKING &&
            (err == OS2_SOCEWOULDBLOCK || err == OS2_SOCEAGAIN)) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("resource temporarily unavailable");
            }
            return NULL;
        }
        if (err_msg != NULL) {
            solo_socket_set_err_fmt(err_msg, "accept() failed (error %d)", err);
        }
        return NULL;
    }

    if (server->mode == SOLO_SOCK_NONBLOCKING) {
        os2_apply_nb(client_fd, server->mode);
    }

    if (client_ip != NULL && ip_len > 0) {
        unsigned long ip = sin.sin_addr.s_addr;
        snprintf(client_ip, ip_len, "%u.%u.%u.%u", (unsigned int)(ip & 0xFF),
                 (unsigned int)((ip >> 8) & 0xFF), (unsigned int)((ip >> 16) & 0xFF),
                 (unsigned int)((ip >> 24) & 0xFF));
    }

    if (client_port != NULL) {
        *client_port = (int)OS2_NTOHS(sin.sin_port);
    }

    client_sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (client_sock == NULL) {
        if (g_tcpip.soclose != NULL) {
            g_tcpip.soclose(client_fd);
        }
        solo_socket_set_err(err_msg, "out of memory");
        return NULL;
    }

    client_sock->fd = client_fd;
    client_sock->type = SOLO_SOCK_TCP;
    client_sock->mode = server->mode;
    client_sock->timeout_ms = server->timeout_ms;
    return client_sock;
}

solo_socket_status_t solo_socket_backend_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    int ret;
    if (sent != NULL) {
        *sent = 0;
    }
    if (!SOLO_SOCK_IS_VALID(sock) || buf == NULL || g_tcpip.send == NULL) {
        return SOLO_SOCK_ERROR;
    }
    ret = g_tcpip.send(sock->fd, buf, len, 0);
    if (ret < 0) {
        int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
        if (err == OS2_SOCEWOULDBLOCK || err == OS2_SOCEAGAIN) {
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
    if (!SOLO_SOCK_IS_VALID(sock) || buf == NULL || max_len == 0 || g_tcpip.recv == NULL) {
        return SOLO_SOCK_ERROR;
    }
    n = g_tcpip.recv(sock->fd, buf, max_len, 0);
    if (n < 0) {
        int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
        if (err == OS2_SOCEWOULDBLOCK || err == OS2_SOCEAGAIN) {
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

solo_socket_status_t solo_socket_sendto(solo_socket_t *sock, const char *buf, size_t len,
                                        const char *host, int port, size_t *sent)
{
    struct os2_sockaddr_in sin;
    unsigned long ip;
    int ret;

    if (sent != NULL) {
        *sent = 0;
    }
    if (!SOLO_SOCK_IS_VALID(sock) || buf == NULL || host == NULL || g_tcpip.sendto == NULL) {
        return SOLO_SOCK_ERROR;
    }

    if (os2_resolve_host(host, &ip, NULL) != 0) {
        return SOLO_SOCK_ERROR;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = OS2_AF_INET;
    sin.sin_port = OS2_HTONS((unsigned short)port);
    sin.sin_addr.s_addr = ip;

    ret = g_tcpip.sendto(sock->fd, buf, len, 0, &sin, sizeof(sin));
    if (ret < 0) {
        int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
        if (err == OS2_SOCEWOULDBLOCK || err == OS2_SOCEAGAIN) {
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
    struct os2_sockaddr_in sin;
    int addrlen = sizeof(sin);
    int n;

    if (received != NULL) {
        *received = 0;
    }
    if (!SOLO_SOCK_IS_VALID(sock) || buf == NULL || max_len == 0 || g_tcpip.recvfrom == NULL) {
        return SOLO_SOCK_ERROR;
    }

    memset(&sin, 0, sizeof(sin));
    n = g_tcpip.recvfrom(sock->fd, buf, max_len, 0, &sin, &addrlen);
    if (n < 0) {
        int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
        if (err == OS2_SOCEWOULDBLOCK || err == OS2_SOCEAGAIN) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    if (n == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
    }

    if (src_ip != NULL && ip_len > 0) {
        unsigned long ip = sin.sin_addr.s_addr;
        snprintf(src_ip, ip_len, "%u.%u.%u.%u", (unsigned int)(ip & 0xFF),
                 (unsigned int)((ip >> 8) & 0xFF), (unsigned int)((ip >> 16) & 0xFF),
                 (unsigned int)((ip >> 24) & 0xFF));
    }

    if (src_port != NULL) {
        *src_port = (int)OS2_NTOHS(sin.sin_port);
    }

    if (received != NULL) {
        *received = (size_t)n;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_backend_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    if (!SOLO_SOCK_IS_VALID(sock)) {
        solo_socket_set_err(err_msg, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }

    if (os2_apply_nb(sock->fd, mode) < 0) {
        solo_socket_set_err(err_msg, "failed to set socket non-blocking mode");
        return SOLO_SOCK_ERROR;
    }

    sock->mode = mode;
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_backend_set_timeout(solo_socket_t *sock, int timeout_ms, char **err_msg)
{
    struct os2_timeval tv;
    long sec, usec;

    if (!SOLO_SOCK_IS_VALID(sock)) {
        solo_socket_set_err(err_msg, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }

    timeout_ms = solo_socket_normalize_timeout(timeout_ms);
    solo_socket_ms_to_timeval(timeout_ms, &sec, &usec);
    tv.tv_sec = sec;
    tv.tv_usec = usec;

    if (g_tcpip.setsockopt != NULL) {
        g_tcpip.setsockopt(sock->fd, OS2_SOL_SOCKET, OS2_SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
        g_tcpip.setsockopt(sock->fd, OS2_SOL_SOCKET, OS2_SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
    }

    sock->timeout_ms = timeout_ms;
    return SOLO_SOCK_OK;
}

int solo_socket_backend_poll(solo_pollfd_t *fds, size_t nfds, int timeout_ms)
{
    int *socks;
    size_t *read_map;
    size_t *write_map;
    size_t *err_map;
    int r_cnt = 0, w_cnt = 0, e_cnt = 0;
    long to;
    int rc;
    int count = 0;
    size_t i;

    if (fds == NULL && nfds > 0) return SOLO_SOCK_ERROR;
    if (g_tcpip.os2_select == NULL) return SOLO_SOCK_ERROR;

    for (i = 0; i < nfds; i++) {
        fds[i].revents = 0;
    }

    if (nfds == 0) return 0;

    socks = (int *)malloc(sizeof(int) * nfds * 3);
    read_map = (size_t *)malloc(sizeof(size_t) * nfds);
    write_map = (size_t *)malloc(sizeof(size_t) * nfds);
    err_map = (size_t *)malloc(sizeof(size_t) * nfds);
    if (!socks || !read_map || !write_map || !err_map) {
        free(socks);
        free(read_map);
        free(write_map);
        free(err_map);
        return SOLO_SOCK_ERROR;
    }

    for (i = 0; i < nfds; i++) {
        if (fds[i].sock != NULL && fds[i].sock->fd >= 0) {
            if (fds[i].events & SOLO_SOCK_POLL_READ) {
                read_map[r_cnt] = i;
                r_cnt++;
            }
        }
    }
    for (i = 0; i < nfds; i++) {
        if (fds[i].sock != NULL && fds[i].sock->fd >= 0) {
            if (fds[i].events & SOLO_SOCK_POLL_WRITE) {
                write_map[w_cnt] = i;
                w_cnt++;
            }
        }
    }
    for (i = 0; i < nfds; i++) {
        if (fds[i].sock != NULL && fds[i].sock->fd >= 0) {
            if (fds[i].events & SOLO_SOCK_POLL_ERR) {
                err_map[e_cnt] = i;
                e_cnt++;
            }
        }
    }

    if (r_cnt == 0 && w_cnt == 0 && e_cnt == 0) {
        free(socks);
        free(read_map);
        free(write_map);
        free(err_map);
        return 0;
    }

    for (i = 0; i < (size_t)r_cnt; i++) {
        socks[i] = fds[read_map[i]].sock->fd;
    }
    for (i = 0; i < (size_t)w_cnt; i++) {
        socks[r_cnt + i] = fds[write_map[i]].sock->fd;
    }
    for (i = 0; i < (size_t)e_cnt; i++) {
        socks[r_cnt + w_cnt + i] = fds[err_map[i]].sock->fd;
    }

    to = (timeout_ms >= 0) ? timeout_ms : -1;
    rc = g_tcpip.os2_select(socks, r_cnt, w_cnt, e_cnt, to);
    if (rc < 0) {
        free(socks);
        free(read_map);
        free(write_map);
        free(err_map);
        return SOLO_SOCK_ERROR;
    }
    if (rc == 0) {
        free(socks);
        free(read_map);
        free(write_map);
        free(err_map);
        return 0;
    }

    for (i = 0; i < (size_t)r_cnt; i++) {
        if (socks[i] != -1) {
            fds[read_map[i]].revents |= SOLO_SOCK_POLL_READ;
        }
    }
    for (i = 0; i < (size_t)w_cnt; i++) {
        if (socks[r_cnt + i] != -1) {
            fds[write_map[i]].revents |= SOLO_SOCK_POLL_WRITE;
        }
    }
    for (i = 0; i < (size_t)e_cnt; i++) {
        if (socks[r_cnt + w_cnt + i] != -1) {
            fds[err_map[i]].revents |= SOLO_SOCK_POLL_ERR;
        }
    }

    for (i = 0; i < nfds; i++) {
        if (fds[i].revents != 0) {
            count++;
        }
    }

    free(socks);
    free(read_map);
    free(write_map);
    free(err_map);
    return count;
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_os2;
#endif /* SOLO_IS_OS2 */
