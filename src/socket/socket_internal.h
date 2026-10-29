/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_SOCKET_INTERNAL_H
#define SOLO_SOCKET_INTERNAL_H

#include <solo/platform.h>
#include <solo/socket.h>

#include <stddef.h>

#if SOLO_IS_WINDOWS
#include <winsock2.h>
#include <windows.h>
#define SOLO_SOCK_HANDLE_TYPE SOCKET
#define SOLO_INVALID_SOCK_HANDLE INVALID_SOCKET
#else
#define SOLO_SOCK_HANDLE_TYPE int
#define SOLO_INVALID_SOCK_HANDLE (-1)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal structure definition for solo_socket.
 * Shared across core engine and all OS platform drivers.
 */
struct solo_socket {
#if SOLO_IS_PLAN9
    int data_fd;      /* /net/<proto>/<conv_id>/data */
    int ctl_fd;       /* /net/<proto>/<conv_id>/ctl */
    char conv_id[16]; /* Conversation line number (e.g. "42") */
    int is_server;    /* 1 if server socket, 0 if client */
#else
    SOLO_SOCK_HANDLE_TYPE fd;
#endif
    solo_socket_type_t type;
    solo_socket_mode_t mode;
    int timeout_ms;
};

#if SOLO_IS_PLAN9
#define SOLO_SOCK_IS_VALID(s) ((s) != NULL && (s)->data_fd >= 0)
#define SOLO_SOCK_FD(s)       ((s) ? (s)->data_fd : -1)
#elif SOLO_IS_WINDOWS
#define SOLO_SOCK_IS_VALID(s) ((s) != NULL && (s)->fd != INVALID_SOCKET)
#define SOLO_SOCK_FD(s)       ((s) ? (int)(s)->fd : -1)
#else
#define SOLO_SOCK_IS_VALID(s) ((s) != NULL && (s)->fd >= 0)
#define SOLO_SOCK_FD(s)       ((s) ? (s)->fd : -1)
#endif

/* =============================================================================
 * Shared Address Engine Helpers
 * ============================================================================= */
const char *solo_socket_normalize_bind_host(const char *host);
const char *solo_socket_normalize_connect_host(const char *host);
int solo_socket_parse_ipv4(const char *host, unsigned long *out_ip_be);
int solo_socket_format_ipv4(unsigned long ip_be, char *buf, size_t len);
int solo_socket_format_port(int port, char *buf, size_t len);

/* =============================================================================
 * Shared Timeout Engine Helpers
 * ============================================================================= */
int solo_socket_normalize_timeout(int timeout_ms);
void solo_socket_ms_to_timeval(int timeout_ms, long *sec, long *usec);

/* =============================================================================
 * Shared Error Reporting Helpers
 * ============================================================================= */
void solo_socket_set_err(char **err_msg, const char *msg);
void solo_socket_set_err_fmt(char **err_msg, const char *fmt, ...);

/* =============================================================================
 * Driver SPI (Service Provider Interface) for OS Backends
 * ============================================================================= */
void solo_socket_backend_close(solo_socket_t *sock);
solo_socket_status_t solo_socket_backend_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent);
solo_socket_status_t solo_socket_backend_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg);
solo_socket_status_t solo_socket_backend_set_timeout(solo_socket_t *sock, int timeout_ms, char **err_msg);
int solo_socket_backend_poll(solo_pollfd_t *fds, size_t nfds, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_SOCKET_INTERNAL_H */
