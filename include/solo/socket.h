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
 * Connects to a remote TCP host:port.
 * Returns socket handle on success, NULL on failure with err_msg populated.
 */
solo_socket_t *solo_socket_connect(const char *host, int port, char **err_msg);

/*
 * Binds a server TCP socket to host:port and starts listening with backlog.
 * Returns server socket handle on success, NULL on failure with err_msg populated.
 */
solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, char **err_msg);

/*
 * Accepts an incoming client connection on a listening server socket.
 * Returns client socket handle on success, NULL on failure with err_msg populated.
 * client_ip and client_port are optional output parameters.
 */
solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                        int *client_port, char **err_msg);

/*
 * Reads up to max_len bytes from the socket.
 * Returns number of bytes read (> 0), 0 on orderly peer disconnect (EOF),
 * or -1 on error.
 */
int solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len);

/*
 * Writes len bytes from buf to the socket.
 * Returns number of bytes written (>= 0) or -1 on error.
 */
int solo_socket_write(solo_socket_t *sock, const char *buf, size_t len);

/*
 * Closes the socket and frees associated memory resources.
 */
void solo_socket_close(solo_socket_t *sock);

#ifdef __cplusplus
}
#endif


#define SOLO_SOCK_BLOCKING    0
#define SOLO_SOCK_NONBLOCKING 1

#define SOLO_SOCK_OK          0
#define SOLO_SOCK_ERROR      -1
#define SOLO_SOCK_AGAIN      -2
#define SOLO_SOCK_CLOSED     -3

/*
 * Sets socket blocking or non-blocking mode.
 * Returns 0 on success, or -1 on error with err_msg populated.
 */
int solo_socket_set_mode(solo_socket_t *sock, int mode, char **err_msg);

#endif /* SOLO_SOCKET_H */
