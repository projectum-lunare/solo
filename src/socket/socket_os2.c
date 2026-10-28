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

struct solo_socket {
    int fd;
};

/*
 * Standalone BSD socket definitions for OS/2 32-bit.
 * Intentionally declared locally without including OS/2 TCP/IP headers
 * to prevent Open Watcom from inserting '#pragma library("tcpip32.lib")',
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

#define OS2_AF_INET      2
#define OS2_SOCK_STREAM  1
#define OS2_SOL_SOCKET   0xFFFF
#define OS2_SO_REUSEADDR 0x0004
#define OS2_INADDR_ANY   0x00000000UL
#define OS2_INADDR_NONE  0xFFFFFFFFUL

#define OS2_HTONS(x) ((((unsigned short)(x) & 0xFF) << 8) | (((unsigned short)(x) >> 8) & 0xFF))
#define OS2_NTOHS(x) OS2_HTONS(x)

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
typedef int(_System *pfn_soclose_t)(int s);
typedef int(_System *pfn_setsockopt_t)(int s, int level, int optname, const void *optval,
                                       int optlen);
typedef struct os2_hostent *(_System *pfn_gethostbyname_t)(const char *name);

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
    pfn_soclose_t soclose;
    pfn_setsockopt_t setsockopt;
    pfn_gethostbyname_t gethostbyname;
} g_tcpip;

/*
 * Self-contained IPv4 dotted-decimal address parser.
 */
static unsigned long parse_ipv4(const char *s)
{
    unsigned int a, b, c, d;
    char tail[2];
    if (s == NULL || s[0] == '\0')
        return OS2_INADDR_NONE;
    if (sscanf(s, "%u.%u.%u.%u%1s", &a, &b, &c, &d, tail) == 4) {
        if (a <= 255 && b <= 255 && c <= 255 && d <= 255) {
            return ((unsigned long)a) | (((unsigned long)b) << 8) | (((unsigned long)c) << 16) |
                   (((unsigned long)d) << 24);
        }
    }
    return OS2_INADDR_NONE;
}

/*
 * Dynamically loads the OS/2 TCP/IP stack at runtime.
 * Tries "SO32DLL" (OS/2 Warp 3 / 2.x standard sockets DLL) first,
 * falling back to "TCPIP32" (OS/2 Warp 4 / Merlin stack).
 * Returns 0 on success, or -1 on failure with err_msg set.
 */
static int os2_tcpip_init(char **err_msg)
{
    char errbuf[128];
    APIRET rc;

    if (g_tcpip.initialized) {
        return 0;
    }
    if (g_tcpip.failed) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup(
                "OS/2 TCP/IP stack (SO32DLL/TCPIP32) is not installed or failed to load");
        }
        return -1;
    }

    /* 1. Try SO32DLL first (OS/2 Warp 3 / 2.x 32-bit sockets DLL) */
    rc = DosLoadModule(errbuf, sizeof(errbuf), "SO32DLL", &g_tcpip.hmod);
    if (rc != 0) {
        /* 2. Fallback to TCPIP32 (OS/2 Warp 4+ / 32-bit stack) */
        rc = DosLoadModule(errbuf, sizeof(errbuf), "TCPIP32", &g_tcpip.hmod);
    }

    if (rc != 0) {
        g_tcpip.failed = 1;
        if (err_msg != NULL) {
            *err_msg = safe_strdup("OS/2 TCP/IP stack (SO32DLL/TCPIP32) is not installed");
        }
        return -1;
    }

    /* 3. Resolve entry points via DosQueryProcAddr (ordinal = 0 for name lookup) */
    DosQueryProcAddr(g_tcpip.hmod, 0, "sock_init", (PFN *)&g_tcpip.sock_init);
    DosQueryProcAddr(g_tcpip.hmod, 0, "sock_errno", (PFN *)&g_tcpip.sock_errno);
    DosQueryProcAddr(g_tcpip.hmod, 0, "socket", (PFN *)&g_tcpip.socket);
    DosQueryProcAddr(g_tcpip.hmod, 0, "connect", (PFN *)&g_tcpip.connect);
    DosQueryProcAddr(g_tcpip.hmod, 0, "bind", (PFN *)&g_tcpip.bind);
    DosQueryProcAddr(g_tcpip.hmod, 0, "listen", (PFN *)&g_tcpip.listen);
    DosQueryProcAddr(g_tcpip.hmod, 0, "accept", (PFN *)&g_tcpip.accept);
    DosQueryProcAddr(g_tcpip.hmod, 0, "send", (PFN *)&g_tcpip.send);
    DosQueryProcAddr(g_tcpip.hmod, 0, "recv", (PFN *)&g_tcpip.recv);
    DosQueryProcAddr(g_tcpip.hmod, 0, "soclose", (PFN *)&g_tcpip.soclose);
    DosQueryProcAddr(g_tcpip.hmod, 0, "setsockopt", (PFN *)&g_tcpip.setsockopt);
    DosQueryProcAddr(g_tcpip.hmod, 0, "gethostbyname", (PFN *)&g_tcpip.gethostbyname);

    if (g_tcpip.socket == NULL || g_tcpip.soclose == NULL || g_tcpip.connect == NULL) {
        g_tcpip.failed = 1;
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Failed to resolve socket API functions in OS/2 TCP/IP stack");
        }
        return -1;
    }

    /* 4. Call sock_init if present */
    if (g_tcpip.sock_init != NULL) {
        g_tcpip.sock_init();
    }

    g_tcpip.initialized = 1;
    return 0;
}

solo_socket_t *solo_socket_connect(const char *host, int port, char **err_msg)
{
    int fd;
    struct os2_sockaddr_in sin;
    unsigned long ip;
    solo_socket_t *sock;

    if (os2_tcpip_init(err_msg) != 0) {
        return NULL;
    }

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = OS2_AF_INET;
    sin.sin_port = OS2_HTONS((unsigned short)port);

    ip = parse_ipv4(host);
    if (ip == OS2_INADDR_NONE && strcmp(host, "255.255.255.255") != 0) {
        if (g_tcpip.gethostbyname != NULL) {
            struct os2_hostent *he = g_tcpip.gethostbyname(host);
            if (he != NULL && he->h_addr_list != NULL && he->h_addr_list[0] != NULL) {
                memcpy(&ip, he->h_addr_list[0], sizeof(ip));
            } else {
                if (err_msg != NULL) {
                    *err_msg = safe_strdup("cannot resolve hostname");
                }
                return NULL;
            }
        } else {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("cannot resolve hostname (gethostbyname unavailable)");
            }
            return NULL;
        }
    }

    sin.sin_addr.s_addr = ip;

    fd = g_tcpip.socket(OS2_AF_INET, OS2_SOCK_STREAM, 0);
    if (fd < 0) {
        if (err_msg != NULL) {
            char buf[64];
            int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
            snprintf(buf, sizeof(buf), "socket() failed (error %d)", err);
            *err_msg = safe_strdup(buf);
        }
        return NULL;
    }

    if (g_tcpip.connect(fd, &sin, sizeof(sin)) < 0) {
        if (err_msg != NULL) {
            char buf[64];
            int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
            snprintf(buf, sizeof(buf), "connect() failed (error %d)", err);
            *err_msg = safe_strdup(buf);
        }
        g_tcpip.soclose(fd);
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        g_tcpip.soclose(fd);
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
    struct os2_sockaddr_in sin;
    unsigned long ip = OS2_INADDR_ANY;
    int opt = 1;
    solo_socket_t *sock;

    if (os2_tcpip_init(err_msg) != 0) {
        return NULL;
    }

    if (g_tcpip.bind == NULL || g_tcpip.listen == NULL) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("bind/listen not supported by OS/2 TCP/IP stack");
        }
        return NULL;
    }

    if (host != NULL && host[0] != '\0' && strcmp(host, "0.0.0.0") != 0 && strcmp(host, "*") != 0) {
        ip = parse_ipv4(host);
        if (ip == OS2_INADDR_NONE && strcmp(host, "255.255.255.255") != 0) {
            if (g_tcpip.gethostbyname != NULL) {
                struct os2_hostent *he = g_tcpip.gethostbyname(host);
                if (he != NULL && he->h_addr_list != NULL && he->h_addr_list[0] != NULL) {
                    memcpy(&ip, he->h_addr_list[0], sizeof(ip));
                } else {
                    if (err_msg != NULL) {
                        *err_msg = safe_strdup("cannot resolve bind hostname");
                    }
                    return NULL;
                }
            } else {
                if (err_msg != NULL) {
                    *err_msg = safe_strdup("cannot resolve bind hostname");
                }
                return NULL;
            }
        }
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = OS2_AF_INET;
    sin.sin_port = OS2_HTONS((unsigned short)port);
    sin.sin_addr.s_addr = ip;

    fd = g_tcpip.socket(OS2_AF_INET, OS2_SOCK_STREAM, 0);
    if (fd < 0) {
        if (err_msg != NULL) {
            char buf[64];
            int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
            snprintf(buf, sizeof(buf), "socket() failed (error %d)", err);
            *err_msg = safe_strdup(buf);
        }
        return NULL;
    }

    if (g_tcpip.setsockopt != NULL) {
        g_tcpip.setsockopt(fd, OS2_SOL_SOCKET, OS2_SO_REUSEADDR, &opt, sizeof(opt));
    }

    if (g_tcpip.bind(fd, &sin, sizeof(sin)) < 0) {
        if (err_msg != NULL) {
            char buf[64];
            int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
            snprintf(buf, sizeof(buf), "bind() failed (error %d)", err);
            *err_msg = safe_strdup(buf);
        }
        g_tcpip.soclose(fd);
        return NULL;
    }

    if (g_tcpip.listen(fd, backlog > 0 ? backlog : 5) < 0) {
        if (err_msg != NULL) {
            char buf[64];
            int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
            snprintf(buf, sizeof(buf), "listen() failed (error %d)", err);
            *err_msg = safe_strdup(buf);
        }
        g_tcpip.soclose(fd);
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        g_tcpip.soclose(fd);
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
    struct os2_sockaddr_in sin;
    int addrlen = sizeof(sin);
    int client_fd;
    solo_socket_t *client_sock;

    if (server == NULL || server->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

    if (os2_tcpip_init(err_msg) != 0) {
        return NULL;
    }

    if (g_tcpip.accept == NULL) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("accept() not supported by OS/2 TCP/IP stack");
        }
        return NULL;
    }

    memset(&sin, 0, sizeof(sin));
    client_fd = g_tcpip.accept(server->fd, &sin, &addrlen);
    if (client_fd < 0) {
        if (err_msg != NULL) {
            char buf[64];
            int err = g_tcpip.sock_errno ? g_tcpip.sock_errno() : 0;
            snprintf(buf, sizeof(buf), "accept() failed (error %d)", err);
            *err_msg = safe_strdup(buf);
        }
        return NULL;
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
        g_tcpip.soclose(client_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory allocating accepted socket handle");
        }
        return NULL;
    }

    client_sock->fd = client_fd;
    return client_sock;
}

int solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len)
{
    if (sock == NULL || sock->fd < 0 || buf == NULL || g_tcpip.recv == NULL) {
        return -1;
    }
    return g_tcpip.recv(sock->fd, buf, max_len, 0);
}

int solo_socket_write(solo_socket_t *sock, const char *buf, size_t len)
{
    if (sock == NULL || sock->fd < 0 || buf == NULL || g_tcpip.send == NULL) {
        return -1;
    }
    return g_tcpip.send(sock->fd, buf, len, 0);
}

void solo_socket_close(solo_socket_t *sock)
{
    if (sock != NULL) {
        if (sock->fd >= 0 && g_tcpip.soclose != NULL) {
            g_tcpip.soclose(sock->fd);
            sock->fd = -1;
        }
        free(sock);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_os2;
#endif /* SOLO_IS_OS2 */

int solo_socket_set_mode(solo_socket_t *sock, int mode, char **err_msg)
{
    (void)sock;
    (void)mode;
    (void)err_msg;
    return 0;
}
