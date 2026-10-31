/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_SOCKET_PAL_H
#define SOLO_SOCKET_PAL_H

#include <solo/platform.h>
#include <solo/types.h>
#include <solo/socket.h>
#include "socket_internal.h"
#include <stddef.h>

/* =============================================================================
 * 1. Windows (WinNT 3.51+, WinCE, MinGW, MSVC)
 * ============================================================================= */
#if SOLO_IS_WINDOWS

/*
 * Redefine FD_SETSIZE before including Winsock headers.
 * On Winsock (since NT 3.51), fd_set is an array of SOCKET handles, not a bitmask.
 * Redefining FD_SETSIZE to 1024 expands the capacity of the select() fallback
 * to 1024 sockets without buffer overflows or ABI breakage on vintage NT systems.
 */
#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#define SOLO_PAL_SOCK_T           SOCKET
#define SOLO_PAL_INVALID_SOCK     INVALID_SOCKET
#define SOLO_PAL_CLOSESOCKET(s)   closesocket(s)
#define SOLO_PAL_SOCK_ERRNO       WSAGetLastError()
#define SOLO_PAL_IS_WOULDBLOCK(e) ((e) == WSAEWOULDBLOCK)
#define SOLO_PAL_IS_INPROGRESS(e) ((e) == WSAEINPROGRESS || (e) == WSAEWOULDBLOCK)
#define SOLO_PAL_IS_INTR(e)       ((e) == WSAEINTR)

static int solo_pal_sock_init(char *err_buf, size_t err_cap)
{
    static int inited = 0;
    if (!inited) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            solo_set_err(err_buf, err_cap, "WSAStartup failed");
            return 0;
        }
        inited = 1;
    }
    return 1;
}

static int solo_pal_set_nonblocking(SOLO_PAL_SOCK_T s, int nb)
{
    u_long mode = (u_long)nb;
    return (ioctlsocket(s, FIONBIO, &mode) == 0) ? 0 : -1;
}

static int solo_pal_set_timeout(SOLO_PAL_SOCK_T s, int timeout_ms)
{
    DWORD ms = (DWORD)timeout_ms;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&ms, sizeof(ms)) != 0) return -1;
    if (setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&ms, sizeof(ms)) != 0) return -1;
    return 0;
}

static void solo_pal_strerror(int err, char *err_buf, size_t err_cap)
{
    solo_set_err_fmt(err_buf, err_cap, "Winsock error %d", err);
}

#define SOLO_PAL_SELECT(n, r, w, e, t) select((int)(n), (r), (w), (e), (t))

/* =============================================================================
 * 2. MS-DOS (WATT-32 TCP/IP stack)
 * ============================================================================= */
#elif SOLO_IS_DOS

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

#define SOLO_PAL_SOCK_T           int
#define SOLO_PAL_INVALID_SOCK     (-1)
#define SOLO_PAL_CLOSESOCKET(s)   close_s(s)
#define SOLO_PAL_SOCK_ERRNO       errno
#define SOLO_PAL_IS_WOULDBLOCK(e) ((e) == EWOULDBLOCK || (e) == EAGAIN)
#define SOLO_PAL_IS_INPROGRESS(e) ((e) == EINPROGRESS)
#define SOLO_PAL_IS_INTR(e)       ((e) == EINTR)

static int solo_pal_sock_init(char *err_buf, size_t err_cap)
{
    static int inited = 0;
    if (!inited) {
        int rc = sock_init();
        if (rc != 0) {
            solo_set_err(err_buf, err_cap, sock_init_err(rc));
            return 0;
        }
        inited = 1;
    }
    return 1;
}

static int solo_pal_set_nonblocking(SOLO_PAL_SOCK_T s, int nb)
{
    return (ioctlsocket(s, FIONBIO, (char *)&nb) == 0) ? 0 : -1;
}

static int solo_pal_set_timeout(SOLO_PAL_SOCK_T s, int timeout_ms)
{
    (void)s;
    (void)timeout_ms;
    return 0;
}

static void solo_pal_strerror(int err, char *err_buf, size_t err_cap)
{
    solo_set_err(err_buf, err_cap, strerror(err));
}

#define SOLO_PAL_SELECT(n, r, w, e, t) select_s((n), (r), (w), (e), (t))

#else /* DOS without WATT-32 */
#error "DOS network requires WATT-32"
#endif

/* =============================================================================
 * 3. OS/2 (Dynamic TCPIP32.DLL / SO32DLL)
 * ============================================================================= */
#elif SOLO_IS_OS2

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct os2_in_addr {
    unsigned long s_addr;
};

struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    struct os2_in_addr sin_addr;
    char sin_zero[8];
};
#define in_addr os2_in_addr

struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

#define AF_INET         2
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOL_SOCKET      0xFFFF
#define SO_REUSEADDR    0x0004
#define SO_RCVTIMEO     0x1006
#define SO_SNDTIMEO     0x1007
#define INADDR_ANY      0x00000000UL
#define INADDR_NONE     0xFFFFFFFFUL
#define FIONBIO         0x8004667eUL

#define htons(x) ((((unsigned short)(x) & 0xFF) << 8) | (((unsigned short)(x) >> 8) & 0xFF))
#define ntohs(x) htons(x)
#define htonl(x) ((((unsigned long)(x) & 0x000000FFUL) << 24) | \
                  (((unsigned long)(x) & 0x0000FF00UL) << 8)  | \
                  (((unsigned long)(x) & 0x00FF0000UL) >> 8)  | \
                  (((unsigned long)(x) & 0xFF000000UL) >> 24))
#define ntohl(x) htonl(x)

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
typedef struct hostent *(_System *pfn_gethostbyname_t)(const char *name);
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
} g_os2_tcpip;

#define SOLO_PAL_SOCK_T           int
#define SOLO_PAL_INVALID_SOCK     (-1)
#define SOLO_PAL_CLOSESOCKET(s)   (g_os2_tcpip.soclose ? g_os2_tcpip.soclose(s) : -1)
#define SOLO_PAL_SOCK_ERRNO       (g_os2_tcpip.sock_errno ? g_os2_tcpip.sock_errno() : 0)
#define SOLO_PAL_IS_WOULDBLOCK(e) ((e) == 35 /*SOCEWOULDBLOCK*/ || (e) == 35 /*SOCEAGAIN*/)
#define SOLO_PAL_IS_INPROGRESS(e) ((e) == 36 /*SOCEINPROGRESS*/)
#define SOLO_PAL_IS_INTR(e)       ((e) == 4 /*SOCEINTR*/)

#define socket(d,t,p)             g_os2_tcpip.socket(d,t,p)
#define connect(s,a,l)            g_os2_tcpip.connect(s,a,l)
#define bind(s,a,l)               g_os2_tcpip.bind(s,a,l)
#define listen(s,b)               g_os2_tcpip.listen(s,b)
#define accept(s,a,l)             g_os2_tcpip.accept(s,a,l)
#define send(s,m,l,f)             g_os2_tcpip.send(s,m,l,f)
#define recv(s,b,l,f)             g_os2_tcpip.recv(s,b,l,f)
#define sendto(s,m,l,f,t,tl)      g_os2_tcpip.sendto(s,m,l,f,t,tl)
#define recvfrom(s,b,l,f,fr,fl)   g_os2_tcpip.recvfrom(s,b,l,f,fr,fl)
#define setsockopt(s,lv,o,v,ol)   g_os2_tcpip.setsockopt(s,lv,o,v,ol)
#define gethostbyname(n)          g_os2_tcpip.gethostbyname(n)

static int solo_pal_sock_init(char *err_buf, size_t err_cap)
{
    char errbuf[128];
    APIRET rc;
    if (g_os2_tcpip.initialized) return 1;
    if (g_os2_tcpip.failed) {
        solo_set_err(err_buf, err_cap, "OS/2 TCP/IP stack is not installed");
        return 0;
    }
    rc = DosLoadModule(errbuf, sizeof(errbuf), "SO32DLL", &g_os2_tcpip.hmod);
    if (rc != 0) {
        rc = DosLoadModule(errbuf, sizeof(errbuf), "TCPIP32", &g_os2_tcpip.hmod);
    }
    if (rc != 0) {
        g_os2_tcpip.failed = 1;
        solo_set_err(err_buf, err_cap, "OS/2 TCP/IP stack (SO32DLL/TCPIP32) is not installed");
        return 0;
    }
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "sock_init", (PFN *)&g_os2_tcpip.sock_init);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "sock_errno", (PFN *)&g_os2_tcpip.sock_errno);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "socket", (PFN *)&g_os2_tcpip.socket);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "connect", (PFN *)&g_os2_tcpip.connect);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "bind", (PFN *)&g_os2_tcpip.bind);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "listen", (PFN *)&g_os2_tcpip.listen);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "accept", (PFN *)&g_os2_tcpip.accept);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "send", (PFN *)&g_os2_tcpip.send);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "recv", (PFN *)&g_os2_tcpip.recv);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "sendto", (PFN *)&g_os2_tcpip.sendto);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "recvfrom", (PFN *)&g_os2_tcpip.recvfrom);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "soclose", (PFN *)&g_os2_tcpip.soclose);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "setsockopt", (PFN *)&g_os2_tcpip.setsockopt);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "gethostbyname", (PFN *)&g_os2_tcpip.gethostbyname);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "os2_ioctl", (PFN *)&g_os2_tcpip.os2_ioctl);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "ioctl", (PFN *)&g_os2_tcpip.ioctl);
    DosQueryProcAddr(g_os2_tcpip.hmod, 0, "os2_select", (PFN *)&g_os2_tcpip.os2_select);

    if (g_os2_tcpip.sock_init != NULL) {
        g_os2_tcpip.sock_init();
    }
    g_os2_tcpip.initialized = 1;
    return 1;
}

static int solo_pal_set_nonblocking(SOLO_PAL_SOCK_T s, int nb)
{
    int val = nb ? 1 : 0;
    if (g_os2_tcpip.os2_ioctl != NULL) {
        return (g_os2_tcpip.os2_ioctl(s, FIONBIO, (char *)&val, sizeof(val)) == 0) ? 0 : -1;
    }
    if (g_os2_tcpip.ioctl != NULL) {
        return (g_os2_tcpip.ioctl(s, FIONBIO, (char *)&val) == 0) ? 0 : -1;
    }
    return -1;
}

static int solo_pal_set_timeout(SOLO_PAL_SOCK_T s, int timeout_ms)
{
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000L;
    if (g_os2_tcpip.setsockopt != NULL) {
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
    }
    return 0;
}

static void solo_pal_strerror(int err, char *err_buf, size_t err_cap)
{
    solo_set_err_fmt(err_buf, err_cap, "OS/2 socket error %d", err);
}

#define SOLO_PAL_HAS_CUSTOM_POLL 1
static int solo_pal_custom_poll(solo_pollfd_t *fds, size_t nfds, int timeout_ms)
{
    int socks[48];
    int r_cnt = 0, w_cnt = 0, e_cnt = 0;
    size_t read_map[16], write_map[16], err_map[16];
    size_t i;
    int rc, count = 0;
    long to;

    if (fds == NULL && nfds > 0) return SOLO_SOCK_ERROR;
    if (nfds == 0) return 0;
    if (g_os2_tcpip.os2_select == NULL) return SOLO_SOCK_ERROR;

    for (i = 0; i < nfds && i < 16; i++) {
        fds[i].revents = 0;
        if (fds[i].sock != NULL && SOLO_SOCK_IS_VALID(fds[i].sock)) {
            if (fds[i].events & SOLO_SOCK_POLL_READ) {
                read_map[r_cnt] = i;
                socks[r_cnt] = fds[i].sock->fd;
                r_cnt++;
            }
        }
    }
    for (i = 0; i < nfds && i < 16; i++) {
        if (fds[i].sock != NULL && SOLO_SOCK_IS_VALID(fds[i].sock)) {
            if (fds[i].events & SOLO_SOCK_POLL_WRITE) {
                write_map[w_cnt] = i;
                socks[r_cnt + w_cnt] = fds[i].sock->fd;
                w_cnt++;
            }
        }
    }
    for (i = 0; i < nfds && i < 16; i++) {
        if (fds[i].sock != NULL && SOLO_SOCK_IS_VALID(fds[i].sock)) {
            err_map[e_cnt] = i;
            socks[r_cnt + w_cnt + e_cnt] = fds[i].sock->fd;
            e_cnt++;
        }
    }

    if (r_cnt == 0 && w_cnt == 0 && e_cnt == 0) return 0;
    to = (timeout_ms >= 0) ? timeout_ms : -1;
    rc = g_os2_tcpip.os2_select(socks, r_cnt, w_cnt, e_cnt, to);
    if (rc < 0) return SOLO_SOCK_ERROR;
    if (rc == 0) return 0;

    for (i = 0; i < (size_t)r_cnt; i++) {
        if (socks[i] != -1) fds[read_map[i]].revents |= SOLO_SOCK_POLL_READ;
    }
    for (i = 0; i < (size_t)w_cnt; i++) {
        if (socks[r_cnt + i] != -1) fds[write_map[i]].revents |= SOLO_SOCK_POLL_WRITE;
    }
    for (i = 0; i < (size_t)e_cnt; i++) {
        if (socks[r_cnt + w_cnt + i] != -1) fds[err_map[i]].revents |= SOLO_SOCK_POLL_ERR;
    }
    for (i = 0; i < nfds; i++) {
        if (fds[i].revents != 0) count++;
    }
    return count;
}

/* =============================================================================
 * 4. Amiga (AmiTCP / bsdsocket.library)
 * ============================================================================= */
#elif SOLO_IS_AMIGA

#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#ifndef FIONBIO
#define FIONBIO 0x8004667e
#endif

typedef unsigned long socklen_t;

extern int CloseSocket(int);
extern int IoctlSocket(int, unsigned long, char *);
extern int WaitSelect(int, void *, void *, void *, struct timeval *, unsigned long *);
extern int Errno(void);

extern struct Library *SocketBase;

#define SOLO_PAL_SOCK_T           int
#define SOLO_PAL_INVALID_SOCK     (-1)
#define SOLO_PAL_CLOSESOCKET(s)   CloseSocket(s)
#define SOLO_PAL_SOCK_ERRNO       Errno()
#define SOLO_PAL_IS_WOULDBLOCK(e) ((e) == EWOULDBLOCK || (e) == EAGAIN)
#define SOLO_PAL_IS_INPROGRESS(e) ((e) == EINPROGRESS)
#define SOLO_PAL_IS_INTR(e)       ((e) == EINTR)

static int solo_pal_sock_init(char *err_buf, size_t err_cap)
{
    if (SocketBase == NULL) {
        SocketBase = OpenLibrary("bsdsocket.library", 4);
        if (SocketBase == NULL) {
            SocketBase = OpenLibrary("bsdsocket.library", 3);
        }
        if (SocketBase == NULL) {
            solo_set_err(err_buf, err_cap, "bsdsocket.library is not running");
            return 0;
        }
    }
    return 1;
}

static int solo_pal_set_nonblocking(SOLO_PAL_SOCK_T s, int nb)
{
    unsigned long val = nb ? 1 : 0;
    return (IoctlSocket(s, FIONBIO, (char *)&val) == 0) ? 0 : -1;
}

static int solo_pal_set_timeout(SOLO_PAL_SOCK_T s, int timeout_ms)
{
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000L;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
    return 0;
}

static void solo_pal_strerror(int err, char *err_buf, size_t err_cap)
{
    solo_set_err_fmt(err_buf, err_cap, "Amiga socket error %d", err);
}

#define SOLO_PAL_SELECT(n, r, w, e, t) WaitSelect((n), (r), (w), (e), (t), NULL)

/* =============================================================================
 * 5. POSIX / Atari ST MiNT (Standard BSD Sockets)
 * ============================================================================= */
#else

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SOLO_PAL_SOCK_T           int
#define SOLO_PAL_INVALID_SOCK     (-1)
#define SOLO_PAL_CLOSESOCKET(s)   close(s)
#define SOLO_PAL_SOCK_ERRNO       errno
#define SOLO_PAL_IS_WOULDBLOCK(e) ((e) == EWOULDBLOCK || (e) == EAGAIN)
#define SOLO_PAL_IS_INPROGRESS(e) ((e) == EINPROGRESS)
#define SOLO_PAL_IS_INTR(e)       ((e) == EINTR)

static int solo_pal_sock_init(char *err_buf, size_t err_cap)
{
    (void)err_buf;
    (void)err_cap;
    return 1;
}

static int solo_pal_set_nonblocking(SOLO_PAL_SOCK_T s, int nb)
{
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return -1;
    if (nb) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return (fcntl(s, F_SETFL, flags) < 0) ? -1 : 0;
}

static int solo_pal_set_timeout(SOLO_PAL_SOCK_T s, int timeout_ms)
{
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000L;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv, sizeof(tv)) < 0) return -1;
    if (setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const void *)&tv, sizeof(tv)) < 0) return -1;
    return 0;
}

static void solo_pal_strerror(int err, char *err_buf, size_t err_cap)
{
    solo_set_err(err_buf, err_cap, strerror(err));
}

#define SOLO_PAL_SELECT(n, r, w, e, t) select((n), (r), (w), (e), (t))

#endif

#endif /* SOLO_SOCKET_PAL_H */
