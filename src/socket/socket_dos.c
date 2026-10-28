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

#if SOLO_IS_DOS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>

#if defined(USE_WATT32)
#include <tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

struct solo_socket {
    int fd;
};

#if defined(USE_WATT32)

static int watt_init_done = 0;

static int ensure_watt32_init(char **err_msg)
{
    if (!watt_init_done) {
        /*
         * sock_init() initializes the Waterloo TCP (WATT-32) stack,
         * detects the hardware/emulated packet driver at INT 0x60..0x6F,
         * parses WATTCP.CFG and optionally performs DHCP resolution.
         */
        int rc = sock_init();
        if (rc != 0) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup(sock_init_err(rc));
            }
            return 0;
        }
        watt_init_done = 1;
    }
    return 1;
}

solo_socket_t *solo_socket_connect(const char *host, int port, char **err_msg)
{
    struct sockaddr_in sin;
    int fd;
    solo_socket_t *sock;

    if (!ensure_watt32_init(err_msg)) {
        return NULL;
    }

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    sin.sin_addr.s_addr = inet_addr((char *)host);
    if (sin.sin_addr.s_addr == INADDR_NONE) {
        struct hostent *he = gethostbyname((char *)host);
        if (he == NULL || he->h_addr == NULL) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("WATT-32: cannot resolve host name");
            }
            return NULL;
        }
        memcpy(&sin.sin_addr, he->h_addr, he->h_length);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("WATT-32: failed to create socket");
        }
        return NULL;
    }

    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("WATT-32: failed to connect to host");
        }
        close_s(fd);
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        close_s(fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }
    sock->fd = fd;
    return sock;
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, char **err_msg)
{
    struct sockaddr_in sin;
    int fd;
    int opt = 1;
    solo_socket_t *sock;

    if (!ensure_watt32_init(err_msg)) {
        return NULL;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    if (host == NULL || host[0] == '\0' || strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0) {
        sin.sin_addr.s_addr = INADDR_ANY;
    } else {
        sin.sin_addr.s_addr = inet_addr((char *)host);
        if (sin.sin_addr.s_addr == INADDR_NONE) {
            sin.sin_addr.s_addr = INADDR_ANY;
        }
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("WATT-32: failed to create server socket");
        }
        return NULL;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("WATT-32: failed to bind server socket");
        }
        close_s(fd);
        return NULL;
    }

    if (listen(fd, backlog > 0 ? backlog : 5) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("WATT-32: failed to listen on server socket");
        }
        close_s(fd);
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        close_s(fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }
    sock->fd = fd;
    return sock;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                        int *client_port, char **err_msg)
{
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    int client_fd;
    solo_socket_t *client_sock;

    if (server == NULL || server->fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

    client_fd = accept(server->fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("WATT-32: accept failed");
        }
        return NULL;
    }

    if (client_ip != NULL && ip_len > 0) {
        char *ip_str = inet_ntoa(client_addr.sin_addr);
        if (ip_str != NULL) {
            strncpy(client_ip, ip_str, ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        } else {
            strncpy(client_ip, "unknown", ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        }
        if (client_port != NULL) {
            *client_port = (int)ntohs(client_addr.sin_port);
        }
    }

    client_sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (client_sock == NULL) {
        close_s(client_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }
    client_sock->fd = client_fd;
    return client_sock;
}

int solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len)
{
    int n;
    if (sock == NULL || sock->fd < 0 || buf == NULL || max_len == 0) {
        return -1;
    }

    n = read_s(sock->fd, buf, (int)max_len);
    return n;
}

int solo_socket_write(solo_socket_t *sock, const char *buf, size_t len)
{
    size_t total = 0;
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return -1;
    }

    while (total < len) {
        int n = write_s(sock->fd, (char *)(buf + total), (int)(len - total));
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
    }

    return (int)total;
}

void solo_socket_close(solo_socket_t *sock)
{
    if (sock != NULL) {
        if (sock->fd >= 0) {
            close_s(sock->fd);
            sock->fd = -1;
        }
        free(sock);
    }
}

#else /* !defined(USE_WATT32) */

solo_socket_t *solo_socket_connect(const char *host, int port, char **err_msg)
{
    (void)host;
    (void)port;
    if (err_msg != NULL) {
        *err_msg =
            safe_strdup("TCP networking on DOS requires WATT-32 (compile with -DUSE_WATT32)");
    }
    return NULL;
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, char **err_msg)
{
    (void)host;
    (void)port;
    (void)backlog;
    if (err_msg != NULL) {
        *err_msg =
            safe_strdup("TCP networking on DOS requires WATT-32 (compile with -DUSE_WATT32)");
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
        *err_msg =
            safe_strdup("TCP networking on DOS requires WATT-32 (compile with -DUSE_WATT32)");
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

#endif /* USE_WATT32 */

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_dos;
#endif /* SOLO_IS_DOS */

int solo_socket_set_mode(solo_socket_t *sock, int mode, char **err_msg)
{
    (void)sock;
    (void)mode;
    (void)err_msg;
    return 0;
}
