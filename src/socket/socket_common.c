#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
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

#include "socket_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* =============================================================================
 * Shared Address Engine
 * ============================================================================= */

const char *solo_socket_normalize_bind_host(const char *host)
{
    if (host == NULL || host[0] == '\0' ||
        strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0) {
        return NULL;
    }
    return host;
}

const char *solo_socket_normalize_connect_host(const char *host)
{
    if (host == NULL || host[0] == '\0') {
        return "127.0.0.1";
    }
    return host;
}

int solo_socket_parse_ipv4(const char *host, unsigned long *out_ip_be)
{
    unsigned int a, b, c, d;
    char tail[2];

    if (host == NULL || host[0] == '\0') {
        return 0;
    }

    if (sscanf(host, "%u.%u.%u.%u%1s", &a, &b, &c, &d, tail) == 4) {
        if (a <= 255 && b <= 255 && c <= 255 && d <= 255) {
            if (out_ip_be != NULL) {
                *out_ip_be = (((unsigned long)a) << 24) |
                             (((unsigned long)b) << 16) |
                             (((unsigned long)c) << 8)  |
                             ((unsigned long)d);
            }
            return 1;
        }
    }
    return 0;
}

int solo_socket_format_ipv4(unsigned long ip_be, char *buf, size_t len)
{
    unsigned int a = (unsigned int)((ip_be >> 24) & 0xFF);
    unsigned int b = (unsigned int)((ip_be >> 16) & 0xFF);
    unsigned int c = (unsigned int)((ip_be >> 8) & 0xFF);
    unsigned int d = (unsigned int)(ip_be & 0xFF);
    int n;

    if (buf == NULL || len == 0) {
        return -1;
    }

    n = sprintf(buf, "%u.%u.%u.%u", a, b, c, d);
    return (n > 0 && (size_t)n < len) ? 0 : -1;
}

int solo_socket_format_port(int port, char *buf, size_t len)
{
    int n;
    if (buf == NULL || len == 0 || port < 0 || port > 65535) {
        return -1;
    }
    n = sprintf(buf, "%d", port);
    return (n > 0 && (size_t)n < len) ? 0 : -1;
}

/* =============================================================================
 * Shared Timeout Engine
 * ============================================================================= */

int solo_socket_normalize_timeout(int timeout_ms)
{
    return (timeout_ms < 0) ? 0 : timeout_ms;
}

void solo_socket_ms_to_timeval(int timeout_ms, long *sec, long *usec)
{
    if (timeout_ms < 0) {
        timeout_ms = 0;
    }
    if (sec != NULL) {
        *sec = timeout_ms / 1000;
    }
    if (usec != NULL) {
        *usec = (long)(timeout_ms % 1000) * 1000L;
    }
}

/* =============================================================================
 * Shared Error Reporting
 * ============================================================================= */

void solo_socket_set_err(char **err_msg, const char *msg)
{
    if (err_msg != NULL) {
        if (*err_msg != NULL) {
            free(*err_msg);
            *err_msg = NULL;
        }
        if (msg != NULL) {
            *err_msg = safe_strdup(msg);
        }
    }
}

void solo_socket_set_err_fmt(char **err_msg, const char *fmt, ...)
{
    if (err_msg != NULL) {
        char buf[256];
        va_list ap;
        va_start(ap, fmt);
#if defined(_MSC_VER) && _MSC_VER < 1900
        _vsnprintf(buf, sizeof(buf), fmt, ap);
#else
        vsnprintf(buf, sizeof(buf), fmt, ap);
#endif
        va_end(ap);
        solo_socket_set_err(err_msg, buf);
    }
}

/* =============================================================================
 * Lifecycle & Creation Wrappers
 * ============================================================================= */

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
        solo_socket_backend_close(sock);
        free(sock);
    }
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

solo_socket_t *solo_socket_connect(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    solo_socket_status_t st;
    solo_socket_t *sock = solo_socket_tcp(mode, err_msg);
    if (sock == NULL) {
        return NULL;
    }

    st = solo_socket_connect_sock(sock, host, port, err_msg);
    if (st != SOLO_SOCK_OK && st != SOLO_SOCK_AGAIN) {
        solo_socket_close(sock);
        return NULL;
    }

    return sock;
}

/* =============================================================================
 * Stream & Datagram I/O
 * ============================================================================= */

solo_socket_status_t solo_socket_write(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    return solo_socket_send(sock, buf, len, sent);
}

solo_socket_status_t solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len, size_t *received)
{
    return solo_socket_recv(sock, buf, max_len, received);
}

solo_socket_status_t solo_socket_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    size_t total = 0;
    if (sent != NULL) {
        *sent = 0;
    }
    if (!SOLO_SOCK_IS_VALID(sock) || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }
    if (len == 0) {
        return SOLO_SOCK_OK;
    }

    /* Datagram (UDP) or non-blocking: execute single raw send attempt */
    if (sock->mode == SOLO_SOCK_NONBLOCKING || sock->type == SOLO_SOCK_UDP) {
        size_t n = 0;
        solo_socket_status_t st = solo_socket_backend_send(sock, buf, len, &n);
        if (st == SOLO_SOCK_OK && sent != NULL) {
            *sent = n;
        }
        return st;
    }

    /* Blocking TCP stream send: loop ensuring complete delivery of all bytes */
    while (total < len) {
        size_t chunk = 0;
        solo_socket_status_t st = solo_socket_backend_send(sock, buf + total, len - total, &chunk);
        if (st == SOLO_SOCK_OK) {
            if (chunk == 0) {
                break;
            }
            total += chunk;
        } else if (st == SOLO_SOCK_AGAIN) {
            if (sock->timeout_ms > 0) {
                int ev = solo_socket_wait(sock, SOLO_SOCK_POLL_WRITE, sock->timeout_ms);
                if (ev <= 0) {
                    if (sent != NULL) {
                        *sent = total;
                    }
                    return (ev == 0) ? SOLO_SOCK_AGAIN : SOLO_SOCK_ERROR;
                }
            }
            continue;
        } else {
            if (sent != NULL) {
                *sent = total;
            }
            return SOLO_SOCK_ERROR;
        }
    }

    if (sent != NULL) {
        *sent = total;
    }
    return SOLO_SOCK_OK;
}

/* =============================================================================
 * Mode, Timeout, Polling & Properties
 * ============================================================================= */

solo_socket_mode_t solo_socket_get_mode(const solo_socket_t *sock)
{
    return sock ? sock->mode : SOLO_SOCK_BLOCKING;
}

solo_socket_type_t solo_socket_get_type(const solo_socket_t *sock)
{
    return sock ? sock->type : SOLO_SOCK_TCP;
}

int solo_socket_get_fd(const solo_socket_t *sock)
{
    return SOLO_SOCK_FD(sock);
}

solo_socket_status_t solo_socket_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    if (!SOLO_SOCK_IS_VALID(sock)) {
        solo_socket_set_err(err_msg, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }
    if (solo_socket_backend_set_mode(sock, mode, err_msg) != SOLO_SOCK_OK) {
        return SOLO_SOCK_ERROR;
    }
    sock->mode = mode;
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_set_timeout(solo_socket_t *sock, int timeout_ms, char **err_msg)
{
    if (!SOLO_SOCK_IS_VALID(sock)) {
        solo_socket_set_err(err_msg, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }
    timeout_ms = solo_socket_normalize_timeout(timeout_ms);
    if (solo_socket_backend_set_timeout(sock, timeout_ms, err_msg) != SOLO_SOCK_OK) {
        return SOLO_SOCK_ERROR;
    }
    sock->timeout_ms = timeout_ms;
    return SOLO_SOCK_OK;
}

int solo_socket_poll(solo_pollfd_t *fds, size_t nfds, int timeout_ms)
{
    if (fds == NULL && nfds > 0) {
        return SOLO_SOCK_ERROR;
    }
    return solo_socket_backend_poll(fds, nfds, timeout_ms);
}

int solo_socket_wait(solo_socket_t *sock, int events, int timeout_ms)
{
    solo_pollfd_t pfd;
    int ret;

    if (!SOLO_SOCK_IS_VALID(sock)) {
        return SOLO_SOCK_ERROR;
    }

    pfd.sock = sock;
    pfd.events = events;
    pfd.revents = 0;

    ret = solo_socket_poll(&pfd, 1, timeout_ms);
    if (ret < 0) {
        return SOLO_SOCK_ERROR;
    }
    if (ret == 0) {
        return 0; /* Timeout */
    }
    return pfd.revents;
}
