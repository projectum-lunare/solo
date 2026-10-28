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

#if !SOLO_IS_POSIX && !SOLO_IS_WINDOWS && !SOLO_IS_AMIGA && !SOLO_IS_DOS

#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>

solo_socket_t *solo_socket_connect(const char *host, int port, char **err_msg)
{
    (void)host;
    (void)port;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("TCP sockets are not supported on this platform");
    }
    return NULL;
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, char **err_msg)
{
    (void)host;
    (void)port;
    (void)backlog;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("TCP sockets are not supported on this platform");
    }
    return NULL;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                        int *client_port, char **err_msg)
{
    (void)server;
    (void)client_ip;
    (void)ip_len;
    (void)client_port;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("TCP sockets are not supported on this platform");
    }
    return NULL;
}

int solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len)
{
    (void)sock;
    (void)buf;
    (void)max_len;
    return -1;
}

int solo_socket_write(solo_socket_t *sock, const char *buf, size_t len)
{
    (void)sock;
    (void)buf;
    (void)len;
    return -1;
}

void solo_socket_close(solo_socket_t *sock)
{
    (void)sock;
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_stub;
#endif /* !SOLO_IS_POSIX && !SOLO_IS_WINDOWS && !SOLO_IS_AMIGA && !SOLO_IS_DOS */

int solo_socket_set_mode(solo_socket_t *sock, int mode, char **err_msg)
{
    (void)sock;
    (void)mode;
    (void)err_msg;
    return 0;
}
