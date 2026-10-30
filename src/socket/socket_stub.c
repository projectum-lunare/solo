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

#if !SOLO_IS_POSIX && !SOLO_IS_WINDOWS && !SOLO_IS_AMIGA && !SOLO_IS_DOS && !SOLO_IS_OS2 && !SOLO_IS_PLAN9 && !SOLO_IS_KOLIBRI && !SOLO_IS_ATARIST

#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>
#include "socket_internal.h"

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char *err_buf, size_t err_cap)
{
    (void)type;
    (void)mode;
    solo_set_err(err_buf, err_cap, "sockets are not supported on this platform");
    return NULL;
}

void solo_socket_backend_close(solo_socket_t *sock)
{
    (void)sock;
}

solo_socket_status_t solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char *err_buf, size_t err_cap)
{
    (void)sock;
    (void)host;
    (void)port;
    (void)backlog;
    solo_set_err(err_buf, err_cap, "sockets are not supported on this platform");
    return SOLO_SOCK_ERROR;
}

solo_socket_status_t solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char *err_buf, size_t err_cap)
{
    (void)sock;
    (void)host;
    (void)port;
    solo_set_err(err_buf, err_cap, "sockets are not supported on this platform");
    return SOLO_SOCK_ERROR;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                  int *client_port, char *err_buf, size_t err_cap)
{
    (void)server;
    (void)client_ip;
    (void)ip_len;
    (void)client_port;
    solo_set_err(err_buf, err_cap, "sockets are not supported on this platform");
    return NULL;
}

solo_socket_status_t solo_socket_backend_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    (void)sock;
    (void)buf;
    (void)len;
    if (sent != NULL) {
        *sent = 0;
    }
    return SOLO_SOCK_ERROR;
}

solo_socket_status_t solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len, size_t *received)
{
    (void)sock;
    (void)buf;
    (void)max_len;
    if (received != NULL) {
        *received = 0;
    }
    return SOLO_SOCK_ERROR;
}

solo_socket_status_t solo_socket_sendto(solo_socket_t *sock, const char *buf, size_t len,
                                        const char *host, int port, size_t *sent)
{
    (void)sock;
    (void)buf;
    (void)len;
    (void)host;
    (void)port;
    if (sent != NULL) {
        *sent = 0;
    }
    return SOLO_SOCK_ERROR;
}

solo_socket_status_t solo_socket_recvfrom(solo_socket_t *sock, char *buf, size_t max_len,
                                          char *src_ip, size_t ip_len, int *src_port,
                                          size_t *received)
{
    (void)sock;
    (void)buf;
    (void)max_len;
    (void)src_ip;
    (void)ip_len;
    (void)src_port;
    if (received != NULL) {
        *received = 0;
    }
    return SOLO_SOCK_ERROR;
}

solo_socket_status_t solo_socket_backend_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char *err_buf, size_t err_cap)
{
    (void)sock;
    (void)mode;
    (void)err_buf;
    (void)err_cap;
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_backend_set_timeout(solo_socket_t *sock, int timeout_ms, char *err_buf, size_t err_cap)
{
    (void)sock;
    (void)timeout_ms;
    (void)err_buf;
    (void)err_cap;
    return SOLO_SOCK_OK;
}

int solo_socket_backend_poll(solo_pollfd_t *fds, size_t nfds, int timeout_ms)
{
    (void)fds;
    (void)nfds;
    (void)timeout_ms;
    return 0;
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_stub;
#endif /* stub */
