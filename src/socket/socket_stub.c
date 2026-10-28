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

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char **err_msg)
{
    (void)type;
    (void)mode;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("sockets are not supported on this platform");
    }
    return NULL;
}

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
    (void)sock;
}

solo_socket_t *solo_socket_connect(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    (void)host;
    (void)port;
    (void)mode;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("sockets are not supported on this platform");
    }
    return NULL;
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, solo_socket_mode_t mode, char **err_msg)
{
    (void)host;
    (void)port;
    (void)backlog;
    (void)mode;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("sockets are not supported on this platform");
    }
    return NULL;
}

solo_socket_t *solo_socket_udp_bind(const char *host, int port, solo_socket_mode_t mode, char **err_msg)
{
    (void)host;
    (void)port;
    (void)mode;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("sockets are not supported on this platform");
    }
    return NULL;
}

solo_socket_status_t solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char **err_msg)
{
    (void)sock;
    (void)host;
    (void)port;
    (void)backlog;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("sockets are not supported on this platform");
    }
    return SOLO_SOCK_ERROR;
}

solo_socket_status_t solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char **err_msg)
{
    (void)sock;
    (void)host;
    (void)port;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("sockets are not supported on this platform");
    }
    return SOLO_SOCK_ERROR;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                  int *client_port, char **err_msg)
{
    (void)server;
    (void)client_ip;
    (void)ip_len;
    (void)client_port;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("sockets are not supported on this platform");
    }
    return NULL;
}

solo_socket_status_t solo_socket_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
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

solo_socket_status_t solo_socket_write(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    return solo_socket_send(sock, buf, len, sent);
}

solo_socket_status_t solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len, size_t *received)
{
    return solo_socket_recv(sock, buf, max_len, received);
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

solo_socket_status_t solo_socket_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char **err_msg)
{
    (void)sock;
    (void)mode;
    (void)err_msg;
    return SOLO_SOCK_OK;
}

solo_socket_mode_t solo_socket_get_mode(const solo_socket_t *sock)
{
    (void)sock;
    return SOLO_SOCK_BLOCKING;
}

solo_socket_type_t solo_socket_get_type(const solo_socket_t *sock)
{
    (void)sock;
    return SOLO_SOCK_TCP;
}

solo_socket_status_t solo_socket_set_timeout(solo_socket_t *sock, int timeout_ms, char **err_msg)
{
    (void)sock;
    (void)timeout_ms;
    (void)err_msg;
    return SOLO_SOCK_OK;
}

int solo_socket_wait(solo_socket_t *sock, int events, int timeout_ms)
{
    (void)sock;
    (void)events;
    (void)timeout_ms;
    return 0;
}

int solo_socket_get_fd(const solo_socket_t *sock)
{
    (void)sock;
    return -1;
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_stub;
#endif /* stub */
