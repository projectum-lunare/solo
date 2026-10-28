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

#if SOLO_IS_WINDOWS_CE

#include <stdlib.h>
#include <string.h>

#include <solo/socket.h>

#include <winsock.h>

struct solo_socket {
    SOCKET fd;
};

static int wsa_init_count = 0;

static void ensure_wsa_init(void)
{
    if (wsa_init_count == 0) {
        WSADATA wsa;
        /* Windows CE: standard Winsock 1.1 */
        WSAStartup(MAKEWORD(1, 1), &wsa);
    }
    wsa_init_count++;
}

static void ensure_wsa_cleanup(void)
{
    if (wsa_init_count > 0) {
        wsa_init_count--;
        if (wsa_init_count == 0) {
            WSACleanup();
        }
    }
}

solo_socket_t *solo_socket_connect(const char *host, int port, char **err_msg)
{
    SOCKET fd;
    struct sockaddr_in sin;
    unsigned long ip;
    solo_socket_t *sock;

    ensure_wsa_init();

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    ip = inet_addr(host);
    if (ip == (unsigned long)INADDR_NONE && strcmp(host, "255.255.255.255") != 0) {
        struct hostent *he = gethostbyname(host);
        if (he != NULL && he->h_addr_list != NULL && he->h_addr_list[0] != NULL) {
            memcpy(&ip, he->h_addr_list[0], sizeof(ip));
        } else {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("cannot resolve hostname");
            }
            ensure_wsa_cleanup();
            return NULL;
        }
    }
    sin.sin_addr.s_addr = ip;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to create socket");
        }
        ensure_wsa_cleanup();
        return NULL;
    }

    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        closesocket(fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to connect to host");
        }
        ensure_wsa_cleanup();
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        closesocket(fd);
        ensure_wsa_cleanup();
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
    SOCKET fd;
    struct sockaddr_in sin;
    int opt = 1;
    solo_socket_t *sock;

    ensure_wsa_init();

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)port);

    if (host == NULL || strcmp(host, "0.0.0.0") == 0 || strcmp(host, "*") == 0 || host[0] == '\0') {
        sin.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        sin.sin_addr.s_addr = inet_addr(host);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to create server socket");
        }
        ensure_wsa_cleanup();
        return NULL;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to bind server socket");
        }
        closesocket(fd);
        ensure_wsa_cleanup();
        return NULL;
    }

    if (listen(fd, backlog > 0 ? backlog : 5) != 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("failed to listen on server socket");
        }
        closesocket(fd);
        ensure_wsa_cleanup();
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        closesocket(fd);
        ensure_wsa_cleanup();
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
    struct sockaddr_in sin;
    int addr_len = sizeof(sin);
    SOCKET client_fd;
    solo_socket_t *client_sock;

    if (server == NULL || server->fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

    client_fd = accept(server->fd, (struct sockaddr *)&sin, &addr_len);
    if (client_fd == INVALID_SOCKET) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("accept failed");
        }
        return NULL;
    }

    if (client_ip != NULL && ip_len > 0) {
        const char *ip_str = inet_ntoa(sin.sin_addr);
        if (ip_str != NULL) {
            strncpy(client_ip, ip_str, ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        } else {
            strncpy(client_ip, "unknown", ip_len - 1);
            client_ip[ip_len - 1] = '\0';
        }
    }
    if (client_port != NULL) {
        *client_port = (int)ntohs(sin.sin_port);
    }

    client_sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (client_sock == NULL) {
        closesocket(client_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }
    client_sock->fd = client_fd;
    ensure_wsa_init();
    return client_sock;
}

int solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len)
{
    int ret;
    if (sock == NULL || sock->fd == INVALID_SOCKET || buf == NULL || max_len == 0) {
        return -1;
    }

    ret = recv(sock->fd, buf, (int)max_len, 0);
    if (ret == SOCKET_ERROR) {
        return -1;
    }
    return ret;
}

int solo_socket_write(solo_socket_t *sock, const char *buf, size_t len)
{
    int total_sent = 0;
    int ret;

    if (sock == NULL || sock->fd == INVALID_SOCKET || buf == NULL) {
        return -1;
    }

    while ((size_t)total_sent < len) {
        ret = send(sock->fd, buf + total_sent, (int)(len - (size_t)total_sent), 0);
        if (ret == SOCKET_ERROR) {
            return -1;
        }
        if (ret == 0) {
            break;
        }
        total_sent += ret;
    }

    return total_sent;
}

void solo_socket_close(solo_socket_t *sock)
{
    if (sock == NULL)
        return;
    if (sock->fd != INVALID_SOCKET) {
        closesocket(sock->fd);
        sock->fd = INVALID_SOCKET;
        ensure_wsa_cleanup();
    }
    free(sock);
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_socket_windows_ce;
#endif /* SOLO_IS_WINDOWS_CE */

int solo_socket_set_mode(solo_socket_t *sock, int mode, char **err_msg)
{
    u_long nonblocking;
    if (sock == NULL || sock->fd == INVALID_SOCKET) {
        if (err_msg) *err_msg = safe_strdup("Invalid socket handle");
        return -1;
    }
    nonblocking = (mode == SOLO_SOCK_NONBLOCKING) ? 1 : 0;
    if (ioctlsocket(sock->fd, FIONBIO, &nonblocking) != 0) {
        if (err_msg) *err_msg = safe_strdup("ioctlsocket FIONBIO failed");
        return -1;
    }
    return 0;
}
