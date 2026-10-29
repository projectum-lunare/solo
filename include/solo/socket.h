/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_SOCKET_H
#define SOLO_SOCKET_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct solo_socket solo_socket_t;

/*
 * Socket transport / protocol type
 */
typedef enum solo_socket_type {
    SOLO_SOCK_TCP = 1,
    SOLO_SOCK_UDP = 2
} solo_socket_type_t;

#define SOLO_SOCK_STREAM SOLO_SOCK_TCP
#define SOLO_SOCK_DGRAM  SOLO_SOCK_UDP

/*
 * Socket blocking mode (Synchronous vs Asynchronous)
 */
typedef enum solo_socket_mode {
    SOLO_SOCK_BLOCKING    = 0,
    SOLO_SOCK_NONBLOCKING = 1
} solo_socket_mode_t;

/*
 * Status and error codes
 */
typedef enum solo_socket_status {
    SOLO_SOCK_OK     =  0,
    SOLO_SOCK_ERROR  = -1,
    SOLO_SOCK_AGAIN  = -2,
    SOLO_SOCK_CLOSED = -3
} solo_socket_status_t;

/*
 * Readiness polling events
 */
typedef enum solo_socket_event {
    SOLO_SOCK_POLL_READ  = 1 << 0,
    SOLO_SOCK_POLL_WRITE = 1 << 1,
    SOLO_SOCK_POLL_ERR   = 1 << 2
} solo_socket_event_t;

/*
 * Error Reporting & Dynamic Memory Ownership:
 * All socket functions accepting a `char **err_msg` argument will dynamically
 * allocate a null-terminated string describing the failure when an error occurs
 * and `err_msg` is non-NULL.
 * The CALLER assumes ownership of this string and is responsible for freeing it
 * using free().
 * If no error occurs, `*err_msg` is not allocated (set to NULL or unchanged).
 */

/*
 * Lifecycle & Creation
 * On error, returns NULL and populates *err_msg (caller must free()).
 */
solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char **err_msg);
solo_socket_t *solo_socket_tcp(solo_socket_mode_t mode, char **err_msg);
solo_socket_t *solo_socket_udp(solo_socket_mode_t mode, char **err_msg);
void solo_socket_close(solo_socket_t *sock);

/*
 * Quick Setup (Connect & Bind)
 * On error, returns NULL and populates *err_msg (caller must free()).
 */
solo_socket_t *solo_socket_connect(const char *host, int port, solo_socket_mode_t mode, char **err_msg);
solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, solo_socket_mode_t mode, char **err_msg);
solo_socket_t *solo_socket_udp_bind(const char *host, int port, solo_socket_mode_t mode, char **err_msg);

/*
 * Operations on existing socket
 * On error, returns SOLO_SOCK_ERROR (or NULL for accept) and populates *err_msg (caller must free()).
 */
solo_socket_status_t solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char **err_msg);
solo_socket_status_t solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char **err_msg);
solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len, int *client_port, char **err_msg);

/*
 * Stream & Connected Datagram I/O
 * Returns SOLO_SOCK_OK with *sent and *received populated (if non-NULL),
 * SOLO_SOCK_AGAIN if non-blocking operation would block,
 * SOLO_SOCK_CLOSED on orderly peer disconnection (EOF),
 * or SOLO_SOCK_ERROR on failure.
 */
solo_socket_status_t solo_socket_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent);
solo_socket_status_t solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len, size_t *received);
solo_socket_status_t solo_socket_write(solo_socket_t *sock, const char *buf, size_t len, size_t *sent);
solo_socket_status_t solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len, size_t *received);

/*
 * Datagram (UDP) I/O
 */
solo_socket_status_t solo_socket_sendto(solo_socket_t *sock, const char *buf, size_t len,
                                        const char *host, int port, size_t *sent);
solo_socket_status_t solo_socket_recvfrom(solo_socket_t *sock, char *buf, size_t max_len,
                                          char *src_ip, size_t ip_len, int *src_port,
                                          size_t *received);

/*
 * Mode, Timeout & Polling
 * On error, functions accepting err_msg return SOLO_SOCK_ERROR and populate *err_msg (caller must free()).
 */
typedef struct solo_pollfd {
    solo_socket_t *sock;
    int events;      /* Input:  SOLO_SOCK_POLL_READ | SOLO_SOCK_POLL_WRITE */
    int revents;     /* Output: SOLO_SOCK_POLL_READ | SOLO_SOCK_POLL_WRITE | SOLO_SOCK_POLL_ERR */
} solo_pollfd_t;

solo_socket_status_t solo_socket_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg);
solo_socket_mode_t solo_socket_get_mode(const solo_socket_t *sock);
solo_socket_type_t solo_socket_get_type(const solo_socket_t *sock);
solo_socket_status_t solo_socket_set_timeout(solo_socket_t *sock, int timeout_ms, char **err_msg);
int solo_socket_wait(solo_socket_t *sock, int events, int timeout_ms);
int solo_socket_poll(solo_pollfd_t *fds, size_t nfds, int timeout_ms);
int solo_socket_get_fd(const solo_socket_t *sock);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_SOCKET_H */
