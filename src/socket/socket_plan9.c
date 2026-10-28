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

#if SOLO_IS_PLAN9

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <solo/socket.h>

/*
 * Plan 9 native network file-system interface (/net/tcp).
 * In Plan 9, network connections are managed as virtual files:
 *   - /net/tcp/clone: opening allocates a new conversation line and returns its ctl fd.
 *   - /net/tcp/<id>/ctl: control file for commands ("connect <ip>!<port>", "announce *!<port>").
 *   - /net/tcp/<id>/data: bidirectional TCP data stream.
 *   - /net/tcp/<id>/listen: opening blocks until an incoming client connects.
 *   - /net/tcp/<id>/remote: contains remote address and port ("<ip>!<port>").
 */

struct solo_socket {
    int data_fd;      /* File descriptor to /net/tcp/<conv_id>/data */
    int ctl_fd;       /* File descriptor to /net/tcp/<conv_id>/ctl */
    char conv_id[16]; /* Conversation line number (e.g. "42") */
    int is_server;    /* 1 if listening server socket, 0 if connected socket */
};

static void trim_trailing_whitespace(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ')) {
        str[--len] = '\0';
    }
}

/* verb + space + host + '!' + decimal port + NUL. Port fits in 11 digits. */
static int format_net_cmd(char *cmd, size_t cmd_len, const char *verb, const char *host, int port)
{
    size_t need;
    int n;

    if (cmd == NULL || verb == NULL || host == NULL || cmd_len == 0) {
        return -1;
    }
    need = strlen(verb) + 1 + strlen(host) + 1 + 11 + 1;
    if (need > cmd_len) {
        return -1;
    }
    n = sprintf(cmd, "%s %s!%d", verb, host, port);
    if (n < 0 || (size_t)n >= cmd_len) {
        return -1;
    }
    return 0;
}

solo_socket_t *solo_socket_connect(const char *host, int port, char **err_msg)
{
    int ctl_fd;
    int data_fd;
    char conv[16];
    char cmd[256];
    char data_path[64];
    ssize_t n;
    solo_socket_t *sock;

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }

    ctl_fd = open("/net/tcp/clone", O_RDWR);
    if (ctl_fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: cannot open /net/tcp/clone (IP stack mounted?)");
        }
        return NULL;
    }

    n = read(ctl_fd, conv, sizeof(conv) - 1);
    if (n <= 0) {
        close(ctl_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: failed to allocate TCP conversation line");
        }
        return NULL;
    }
    conv[n] = '\0';
    trim_trailing_whitespace(conv);

    if (format_net_cmd(cmd, sizeof(cmd), "connect", host, port) != 0) {
        close(ctl_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: host name is too long");
        }
        return NULL;
    }
    if (write(ctl_fd, cmd, strlen(cmd)) < 0) {
        close(ctl_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: connection to remote host failed");
        }
        return NULL;
    }

    sprintf(data_path, "/net/tcp/%s/data", conv);
    data_fd = open(data_path, O_RDWR);
    if (data_fd < 0) {
        close(ctl_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: failed to open TCP data stream");
        }
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        close(data_fd);
        close(ctl_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }

    sock->data_fd = data_fd;
    sock->ctl_fd = ctl_fd;
    strncpy(sock->conv_id, conv, sizeof(sock->conv_id) - 1);
    sock->conv_id[sizeof(sock->conv_id) - 1] = '\0';
    sock->is_server = 0;
    return sock;
}

solo_socket_t *solo_socket_bind(const char *host, int port, int backlog, char **err_msg)
{
    int ctl_fd;
    char conv[16];
    char cmd[256];
    ssize_t n;
    solo_socket_t *sock;

    SOLO_UNUSED(backlog);

    if (host == NULL || host[0] == '\0' || strcmp(host, "0.0.0.0") == 0) {
        host = "*";
    }

    ctl_fd = open("/net/tcp/clone", O_RDWR);
    if (ctl_fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: cannot open /net/tcp/clone");
        }
        return NULL;
    }

    n = read(ctl_fd, conv, sizeof(conv) - 1);
    if (n <= 0) {
        close(ctl_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: failed to allocate TCP conversation line");
        }
        return NULL;
    }
    conv[n] = '\0';
    trim_trailing_whitespace(conv);

    if (format_net_cmd(cmd, sizeof(cmd), "announce", host, port) != 0) {
        close(ctl_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: host name is too long");
        }
        return NULL;
    }
    if (write(ctl_fd, cmd, strlen(cmd)) < 0) {
        close(ctl_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: announce on port failed");
        }
        return NULL;
    }

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        close(ctl_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }

    sock->data_fd = -1;
    sock->ctl_fd = ctl_fd;
    strncpy(sock->conv_id, conv, sizeof(sock->conv_id) - 1);
    sock->conv_id[sizeof(sock->conv_id) - 1] = '\0';
    sock->is_server = 1;
    return sock;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                        int *client_port, char **err_msg)
{
    char listen_path[64];
    char client_conv[16];
    char client_data_path[64];
    int listen_fd;
    int client_data_fd;
    ssize_t n;
    solo_socket_t *client_sock;

    if (server == NULL || server->ctl_fd < 0 || !server->is_server) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("invalid server socket");
        }
        return NULL;
    }

    sprintf(listen_path, "/net/tcp/%s/listen", server->conv_id);
    listen_fd = open(listen_path, O_RDWR);
    if (listen_fd < 0) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: failed to open /net/tcp/.../listen");
        }
        return NULL;
    }

    n = read(listen_fd, client_conv, sizeof(client_conv) - 1);
    if (n <= 0) {
        close(listen_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: accept failed while reading incoming conversation");
        }
        return NULL;
    }
    client_conv[n] = '\0';
    trim_trailing_whitespace(client_conv);

    sprintf(client_data_path, "/net/tcp/%s/data", client_conv);
    client_data_fd = open(client_data_path, O_RDWR);
    if (client_data_fd < 0) {
        close(listen_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Plan 9: failed to open client data stream");
        }
        return NULL;
    }

    if (client_ip != NULL || client_port != NULL) {
        char remote_path[64];
        char remote_buf[128];
        int rfd;

        sprintf(remote_path, "/net/tcp/%s/remote", client_conv);
        rfd = open(remote_path, O_RDONLY);
        if (rfd >= 0) {
            ssize_t rn = read(rfd, remote_buf, sizeof(remote_buf) - 1);
            close(rfd);
            if (rn > 0) {
                char *excl;
                remote_buf[rn] = '\0';
                trim_trailing_whitespace(remote_buf);

                excl = strchr(remote_buf, '!');
                if (excl != NULL) {
                    *excl = '\0';
                    if (client_port != NULL) {
                        *client_port = atoi(excl + 1);
                    }
                }
                if (client_ip != NULL && ip_len > 0) {
                    strncpy(client_ip, remote_buf, ip_len - 1);
                    client_ip[ip_len - 1] = '\0';
                }
            }
        }
    }

    client_sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (client_sock == NULL) {
        close(client_data_fd);
        close(listen_fd);
        if (err_msg != NULL) {
            *err_msg = safe_strdup("out of memory");
        }
        return NULL;
    }

    client_sock->data_fd = client_data_fd;
    client_sock->ctl_fd = listen_fd;
    strncpy(client_sock->conv_id, client_conv, sizeof(client_sock->conv_id) - 1);
    client_sock->conv_id[sizeof(client_sock->conv_id) - 1] = '\0';
    client_sock->is_server = 0;
    return client_sock;
}

int solo_socket_read(solo_socket_t *sock, char *buf, size_t max_len)
{
    ssize_t n;
    if (sock == NULL || sock->data_fd < 0 || buf == NULL || max_len == 0) {
        return -1;
    }

    n = read(sock->data_fd, buf, max_len);
    if (n < 0) {
        return -1;
    }
    return (int)n;
}

int solo_socket_write(solo_socket_t *sock, const char *buf, size_t len)
{
    ssize_t n;
    if (sock == NULL || sock->data_fd < 0 || buf == NULL) {
        return -1;
    }

    n = write(sock->data_fd, buf, len);
    if (n < 0) {
        return -1;
    }
    return (int)n;
}

void solo_socket_close(solo_socket_t *sock)
{
    if (sock == NULL)
        return;

    if (sock->data_fd >= 0) {
        close(sock->data_fd);
        sock->data_fd = -1;
    }
    if (sock->ctl_fd >= 0) {
        close(sock->ctl_fd);
        sock->ctl_fd = -1;
    }
    free(sock);
}

#else
typedef int make_iso_compilers_happy_socket_plan9;
#endif /* SOLO_IS_PLAN9 */

int solo_socket_set_mode(solo_socket_t *sock, int mode, char **err_msg)
{
    (void)sock;
    (void)mode;
    (void)err_msg;
    return 0;
}
