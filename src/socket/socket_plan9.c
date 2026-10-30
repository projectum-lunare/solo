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
#include <sys/select.h>
#include <sys/time.h>

#include <solo/socket.h>
#include "socket_internal.h"

/*
 * Plan 9 native network file-system interface (/net/tcp and /net/udp).
 * In Plan 9, network connections are managed as virtual files:
 *   - /net/<proto>/clone: opening allocates a new conversation line and returns its ctl fd.
 *   - /net/<proto>/<id>/ctl: control file for commands ("connect <ip>!<port>", "announce *!<port>").
 *   - /net/<proto>/<id>/data: bidirectional TCP or UDP data stream.
 *   - /net/tcp/<id>/listen: opening blocks until an incoming client connects.
 *   - /net/<proto>/<id>/remote: contains remote address and port ("<ip>!<port>").
 */

static void trim_trailing_whitespace(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ')) {
        str[--len] = '\0';
    }
}

static int format_net_cmd(char *cmd, size_t cmd_len, const char *verb, const char *host, int port)
{
    size_t need;
    int n;

    if (cmd == NULL || verb == NULL || host == NULL || cmd_len == 0) {
        return -1;
    }
    if (strcmp(host, "0.0.0.0") == 0) {
        host = "*";
    }
    need = strlen(verb) + 1 + strlen(host) + 1 + 11 + 1;
    if (need > cmd_len) {
        return -1;
    }
    n = snprintf(cmd, cmd_len, "%s %s!%d", verb, host, port);
    if (n < 0 || (size_t)n >= cmd_len) {
        return -1;
    }
    return 0;
}

static const char *proto_name(solo_socket_type_t type)
{
    return (type == SOLO_SOCK_UDP) ? "udp" : "tcp";
}

solo_socket_t *solo_socket_create(solo_socket_type_t type, solo_socket_mode_t mode, char *err_buf, size_t err_cap)
{
    char clone_path[32];
    char conv[16];
    int ctl_fd;
    ssize_t n;
    solo_socket_t *sock;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    snprintf(clone_path, sizeof(clone_path), "/net/%s/clone", proto_name(type));
    ctl_fd = open(clone_path, O_RDWR);
    if (ctl_fd < 0) {
        solo_set_err(err_buf, err_cap, "Plan 9: cannot open clone file (IP stack mounted?)");
        return NULL;
    }

    n = read(ctl_fd, conv, sizeof(conv) - 1);
    if (n <= 0) {
        close(ctl_fd);
        solo_set_err(err_buf, err_cap, "Plan 9: failed to allocate conversation line");
        return NULL;
    }
    conv[n] = '\0';
    trim_trailing_whitespace(conv);

    sock = (solo_socket_t *)malloc(sizeof(solo_socket_t));
    if (sock == NULL) {
        close(ctl_fd);
        solo_set_err(err_buf, err_cap, "out of memory");
        return NULL;
    }

    sock->data_fd = -1;
    sock->ctl_fd = ctl_fd;
    strncpy(sock->conv_id, conv, sizeof(sock->conv_id) - 1);
    sock->conv_id[sizeof(sock->conv_id) - 1] = '\0';
    sock->is_server = 0;
    sock->type = type;
    sock->mode = mode;
    sock->timeout_ms = 0;

    /* For UDP, open the data file immediately so it can be used for sendto/recvfrom */
    if (type == SOLO_SOCK_UDP) {
        char data_path[64];
        snprintf(data_path, sizeof(data_path), "/net/udp/%s/data", conv);
        sock->data_fd = open(data_path, O_RDWR);
        if (sock->data_fd >= 0 && mode == SOLO_SOCK_NONBLOCKING) {
            int flags = fcntl(sock->data_fd, F_GETFL, 0);
            if (flags >= 0) {
                fcntl(sock->data_fd, F_SETFL, flags | O_NONBLOCK);
            }
        }
    }

    return sock;
}

void solo_socket_backend_close(solo_socket_t *sock)
{
    if (sock == NULL) {
        return;
    }
    if (sock->data_fd >= 0) {
        close(sock->data_fd);
        sock->data_fd = -1;
    }
    if (sock->ctl_fd >= 0) {
        close(sock->ctl_fd);
        sock->ctl_fd = -1;
    }
}

solo_socket_status_t solo_socket_bind_sock(solo_socket_t *sock, const char *host, int port, int backlog, char *err_buf, size_t err_cap)
{
    char cmd[256];
    (void)backlog;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (sock == NULL || sock->ctl_fd < 0) {
        solo_set_err(err_buf, err_cap, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }

    host = solo_socket_normalize_bind_host(host);

    if (format_net_cmd(cmd, sizeof(cmd), "announce", host, port) != 0) {
        solo_set_err(err_buf, err_cap, "Plan 9: host name is too long");
        return SOLO_SOCK_ERROR;
    }

    if (write(sock->ctl_fd, cmd, strlen(cmd)) < 0) {
        solo_set_err(err_buf, err_cap, "Plan 9: announce failed");
        return SOLO_SOCK_ERROR;
    }

    if (sock->type == SOLO_SOCK_TCP) {
        sock->is_server = 1;
    } else {
        sock->is_server = 0;
        if (sock->data_fd < 0) {
            char data_path[64];
            snprintf(data_path, sizeof(data_path), "/net/udp/%s/data", sock->conv_id);
            sock->data_fd = open(data_path, O_RDWR);
            if (sock->data_fd >= 0 && sock->mode == SOLO_SOCK_NONBLOCKING) {
                int flags = fcntl(sock->data_fd, F_GETFL, 0);
                if (flags >= 0) {
                    fcntl(sock->data_fd, F_SETFL, flags | O_NONBLOCK);
                }
            }
        }
    }

    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_connect_sock(solo_socket_t *sock, const char *host, int port, char *err_buf, size_t err_cap)
{
    char cmd[256];
    char data_path[64];

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (sock == NULL || sock->ctl_fd < 0) {
        solo_set_err(err_buf, err_cap, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }

    host = solo_socket_normalize_connect_host(host);

    if (format_net_cmd(cmd, sizeof(cmd), "connect", host, port) != 0) {
        solo_set_err(err_buf, err_cap, "Plan 9: host name is too long");
        return SOLO_SOCK_ERROR;
    }

    if (write(sock->ctl_fd, cmd, strlen(cmd)) < 0) {
        if (sock->mode == SOLO_SOCK_NONBLOCKING &&
            (errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EAGAIN)) {
            return SOLO_SOCK_AGAIN;
        }
        solo_set_err(err_buf, err_cap, "Plan 9: connection to remote host failed");
        return SOLO_SOCK_ERROR;
    }

    if (sock->data_fd < 0) {
        snprintf(data_path, sizeof(data_path), "/net/%s/%s/data", proto_name(sock->type), sock->conv_id);
        sock->data_fd = open(data_path, O_RDWR);
        if (sock->data_fd < 0) {
            solo_set_err(err_buf, err_cap, "Plan 9: failed to open data stream");
            return SOLO_SOCK_ERROR;
        }

        if (sock->mode == SOLO_SOCK_NONBLOCKING) {
            int flags = fcntl(sock->data_fd, F_GETFL, 0);
            if (flags >= 0) {
                fcntl(sock->data_fd, F_SETFL, flags | O_NONBLOCK);
            }
        }
    }

    return SOLO_SOCK_OK;
}

solo_socket_t *solo_socket_accept(solo_socket_t *server, char *client_ip, size_t ip_len,
                                  int *client_port, char *err_buf, size_t err_cap)
{
    char listen_path[64];
    char client_conv[16];
    char client_data_path[64];
    int listen_fd;
    int client_data_fd;
    ssize_t n;
    solo_socket_t *client_sock;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (server == NULL || server->ctl_fd < 0 || !server->is_server) {
        solo_set_err(err_buf, err_cap, "invalid server socket");
        return NULL;
    }

    snprintf(listen_path, sizeof(listen_path), "/net/tcp/%s/listen", server->conv_id);
    listen_fd = open(listen_path, O_RDWR);
    if (listen_fd < 0) {
        if (server->mode == SOLO_SOCK_NONBLOCKING && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("resource temporarily unavailable");
            }
            return NULL;
        }
        solo_socket_set_err(err_msg, "Plan 9: failed to open /net/tcp/.../listen");
        return NULL;
    }

    n = read(listen_fd, client_conv, sizeof(client_conv) - 1);
    if (n <= 0) {
        close(listen_fd);
        if (server->mode == SOLO_SOCK_NONBLOCKING && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("resource temporarily unavailable");
            }
            return NULL;
        }
        solo_socket_set_err(err_msg, "Plan 9: accept failed while reading incoming conversation");
        return NULL;
    }
    client_conv[n] = '\0';
    trim_trailing_whitespace(client_conv);

    snprintf(client_data_path, sizeof(client_data_path), "/net/tcp/%s/data", client_conv);
    client_data_fd = open(client_data_path, O_RDWR);
    if (client_data_fd < 0) {
        close(listen_fd);
        solo_socket_set_err(err_msg, "Plan 9: failed to open client data stream");
        return NULL;
    }

    if (server->mode == SOLO_SOCK_NONBLOCKING) {
        int flags = fcntl(client_data_fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(client_data_fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    if (client_ip != NULL || client_port != NULL) {
        char remote_path[64];
        char remote_buf[128];
        int rfd;

        snprintf(remote_path, sizeof(remote_path), "/net/tcp/%s/remote", client_conv);
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
        solo_socket_set_err(err_msg, "out of memory");
        return NULL;
    }

    client_sock->data_fd = client_data_fd;
    client_sock->ctl_fd = listen_fd;
    strncpy(client_sock->conv_id, client_conv, sizeof(client_sock->conv_id) - 1);
    client_sock->conv_id[sizeof(client_sock->conv_id) - 1] = '\0';
    client_sock->is_server = 0;
    client_sock->type = SOLO_SOCK_TCP;
    client_sock->mode = server->mode;
    client_sock->timeout_ms = server->timeout_ms;
    return client_sock;
}

solo_socket_status_t solo_socket_backend_send(solo_socket_t *sock, const char *buf, size_t len, size_t *sent)
{
    ssize_t n;
    if (sent != NULL) {
        *sent = 0;
    }
    if (!SOLO_SOCK_IS_VALID(sock) || buf == NULL) {
        return SOLO_SOCK_ERROR;
    }
    n = write(sock->data_fd, buf, len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    if (sent != NULL) {
        *sent = (size_t)n;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_recv(solo_socket_t *sock, char *buf, size_t max_len, size_t *received)
{
    ssize_t n;
    if (received != NULL) {
        *received = 0;
    }
    if (!SOLO_SOCK_IS_VALID(sock) || buf == NULL || max_len == 0) {
        return SOLO_SOCK_ERROR;
    }
    n = read(sock->data_fd, buf, max_len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SOLO_SOCK_AGAIN;
        }
        return SOLO_SOCK_ERROR;
    }
    if (n == 0 && sock->type == SOLO_SOCK_TCP) {
        return SOLO_SOCK_CLOSED;
    }
    if (received != NULL) {
        *received = (size_t)n;
    }
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_sendto(solo_socket_t *sock, const char *buf, size_t len,
                                        const char *host, int port, size_t *sent)
{
    char cmd[256];
    if (sent != NULL) {
        *sent = 0;
    }
    if (!SOLO_SOCK_IS_VALID(sock) || buf == NULL || host == NULL) {
        return SOLO_SOCK_ERROR;
    }

    if (sock->type == SOLO_SOCK_UDP && sock->ctl_fd >= 0) {
        if (format_net_cmd(cmd, sizeof(cmd), "connect", host, port) == 0) {
            write(sock->ctl_fd, cmd, strlen(cmd));
        }
    }

    return solo_socket_backend_send(sock, buf, len, sent);
}

solo_socket_status_t solo_socket_recvfrom(solo_socket_t *sock, char *buf, size_t max_len,
                                          char *src_ip, size_t ip_len, int *src_port,
                                          size_t *received)
{
    solo_socket_status_t status = solo_socket_recv(sock, buf, max_len, received);
    if (status == SOLO_SOCK_OK) {
        if (src_ip != NULL || src_port != NULL) {
            char remote_path[64];
            char remote_buf[128];
            int rfd;

            snprintf(remote_path, sizeof(remote_path), "/net/%s/%s/remote",
                     proto_name(sock->type), sock->conv_id);
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
                        if (src_port != NULL) {
                            *src_port = atoi(excl + 1);
                        }
                    }
                    if (src_ip != NULL && ip_len > 0) {
                        strncpy(src_ip, remote_buf, ip_len - 1);
                        src_ip[ip_len - 1] = '\0';
                    }
                }
            }
        }
    }
    return status;
}

solo_socket_status_t solo_socket_backend_set_mode(solo_socket_t *sock, solo_socket_mode_t mode, char *err_buf, size_t err_cap)
{
    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (sock == NULL) {
        solo_set_err(err_buf, err_cap, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }

    if (sock->data_fd >= 0) {
        int flags = fcntl(sock->data_fd, F_GETFL, 0);
        if (flags >= 0) {
            if (mode == SOLO_SOCK_NONBLOCKING) {
                flags |= O_NONBLOCK;
            } else {
                flags &= ~O_NONBLOCK;
            }
            fcntl(sock->data_fd, F_SETFL, flags);
        }
    }

    sock->mode = mode;
    return SOLO_SOCK_OK;
}

solo_socket_status_t solo_socket_backend_set_timeout(solo_socket_t *sock, int timeout_ms, char *err_buf, size_t err_cap)
{
    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (sock == NULL) {
        solo_set_err(err_buf, err_cap, "invalid socket handle");
        return SOLO_SOCK_ERROR;
    }
    sock->timeout_ms = solo_socket_normalize_timeout(timeout_ms);
    return SOLO_SOCK_OK;
}

int solo_socket_backend_poll(solo_pollfd_t *fds, size_t nfds, int timeout_ms)
{
    fd_set read_fds, write_fds, except_fds;
    struct timeval tv, *tvp = NULL;
    int max_fd = -1;
    int count = 0;
    int rc;
    size_t i;

    if (fds == NULL && nfds > 0) return SOLO_SOCK_ERROR;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);

    for (i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].sock != NULL && fds[i].sock->data_fd >= 0) {
            int fd = fds[i].sock->data_fd;
            if (fd > max_fd) max_fd = fd;
            if (fds[i].events & SOLO_SOCK_POLL_READ) {
                FD_SET(fd, &read_fds);
            }
            if (fds[i].events & SOLO_SOCK_POLL_WRITE) {
                FD_SET(fd, &write_fds);
            }
            if (fds[i].events & SOLO_SOCK_POLL_ERR) {
                FD_SET(fd, &except_fds);
            }
        }
    }

    if (max_fd < 0) {
        return 0;
    }

    if (timeout_ms >= 0) {
        long sec, usec;
        solo_socket_ms_to_timeval(timeout_ms, &sec, &usec);
        tv.tv_sec = sec;
        tv.tv_usec = usec;
        tvp = &tv;
    }

    rc = select(max_fd + 1, &read_fds, &write_fds, &except_fds, tvp);
    if (rc < 0) {
        if (errno == EINTR) {
            return 0;
        }
        return SOLO_SOCK_ERROR;
    }
    if (rc == 0) {
        return 0;
    }

    for (i = 0; i < nfds; i++) {
        if (fds[i].sock != NULL && fds[i].sock->data_fd >= 0) {
            int fd = fds[i].sock->data_fd;
            if (FD_ISSET(fd, &read_fds)) fds[i].revents |= SOLO_SOCK_POLL_READ;
            if (FD_ISSET(fd, &write_fds)) fds[i].revents |= SOLO_SOCK_POLL_WRITE;
            if (FD_ISSET(fd, &except_fds)) fds[i].revents |= SOLO_SOCK_POLL_ERR;
            if (fds[i].revents != 0) count++;
        }
    }

    return count;
}

#else
typedef int make_iso_compilers_happy_socket_plan9;
#endif /* SOLO_IS_PLAN9 */
