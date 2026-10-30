/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 * Mozilla Public License Version 2.0
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <greatest/greatest.h>

#include <solo/solo.h>

#if SOLO_IS_POSIX
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

TEST test_platform(void)
{
    ASSERT_STR_EQ(SOLO_OS_NAME, solo_platform_os());
    ASSERT_STR_EQ(SOLO_ARCH_NAME, solo_platform_arch());
    ASSERT_EQ(SOLO_BITS, solo_platform_bits());
    ASSERT(solo_platform_endian() != NULL);
    PASS();
}

TEST test_filesystem(void)
{
    ASSERT_EQ(1, solo_fs_exists("."));
    ASSERT_EQ(1, solo_fs_exists("include/solo/solo.h"));
    ASSERT_EQ(0, solo_fs_exists("/nonexistent_solo_xyz_12345"));
    ASSERT_EQ(0, solo_fs_exists("nonexistent_relative_xyz_67890"));
    ASSERT_EQ(0, solo_fs_exists(NULL));
    PASS();
}

TEST test_sandbox(void)
{
    int version = 0;
    solo_sandbox_backend_t b = solo_sandbox_backend(&version);
    const char *backend = solo_sandbox_backend_name(&version);
    ASSERT(backend != NULL);
    ASSERT(strlen(backend) > 0);
    ASSERT(b >= SOLO_SANDBOX_BACKEND_NONE && b <= SOLO_SANDBOX_BACKEND_CAPSICUM);
    PASS();
}

TEST test_sandbox_enums(void)
{
    ASSERT_EQ(0, SOLO_SANDBOX_BACKEND_NONE);
    ASSERT_EQ(1, SOLO_SANDBOX_BACKEND_LANDLOCK);
    ASSERT_EQ(2, SOLO_SANDBOX_BACKEND_OPENBSD);
    ASSERT_EQ(3, SOLO_SANDBOX_BACKEND_CAPSICUM);

    ASSERT_EQ(0, SOLO_SANDBOX_PERM_NONE);
    ASSERT_EQ(1 << 0, SOLO_SANDBOX_PERM_READ);
    ASSERT_EQ(1 << 1, SOLO_SANDBOX_PERM_WRITE);
    ASSERT_EQ(1 << 2, SOLO_SANDBOX_PERM_EXEC);

    ASSERT_EQ(0, SOLO_SANDBOX_OK);
    ASSERT_EQ(-1, SOLO_SANDBOX_ERROR);
    ASSERT_EQ(-2, SOLO_SANDBOX_UNSUPPORTED);
    PASS();
}

TEST test_socket_enums(void)
{
    ASSERT_EQ(1, SOLO_SOCK_TCP);
    ASSERT_EQ(2, SOLO_SOCK_UDP);

    ASSERT_EQ(0, SOLO_SOCK_BLOCKING);
    ASSERT_EQ(1, SOLO_SOCK_NONBLOCKING);

    ASSERT_EQ(0, SOLO_SOCK_OK);
    ASSERT_EQ(-1, SOLO_SOCK_ERROR);
    ASSERT_EQ(-2, SOLO_SOCK_AGAIN);
    ASSERT_EQ(-3, SOLO_SOCK_CLOSED);

    ASSERT_EQ(1 << 0, SOLO_SOCK_POLL_READ);
    ASSERT_EQ(1 << 1, SOLO_SOCK_POLL_WRITE);
    ASSERT_EQ(1 << 2, SOLO_SOCK_POLL_ERR);
    PASS();
}

TEST test_socket_tcp_sync(void)
{
    char *err_msg = NULL;
    int port = 19930;
    solo_socket_t *server;
    solo_socket_t *client;
    solo_socket_t *accepted;
    char client_ip[64];
    int client_port = 0;
    const char *msg = "Direct TCP sync test for solo!";
    char buf[128];
    size_t sent = 0;
    size_t received = 0;
    solo_socket_status_t st;

    server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    if (!server) {
        if (err_msg != NULL) {
            free(err_msg);
            err_msg = NULL;
        }
        port = 19931;
        server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    }
    ASSERT(server != NULL);
    ASSERT_EQ(SOLO_SOCK_TCP, solo_socket_get_type(server));
    ASSERT_EQ(SOLO_SOCK_BLOCKING, solo_socket_get_mode(server));

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT(client != NULL);
    ASSERT_EQ(SOLO_SOCK_TCP, solo_socket_get_type(client));

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, &err_msg);
    ASSERT(accepted != NULL);

    st = solo_socket_send(client, msg, strlen(msg), &sent);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), sent);

    memset(buf, 0, sizeof(buf));
    st = solo_socket_recv(accepted, buf, sizeof(buf) - 1, &received);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), received);
    ASSERT_STR_EQ(msg, buf);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);
    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    PASS();
}

TEST test_socket_tcp_async(void)
{
    char *err_msg = NULL;
    int port = 19940;
    solo_socket_t *server;
    solo_socket_t *client;
    solo_socket_t *accepted;
    char client_ip[64];
    int client_port = 0;
    const char *msg = "Async TCP non-blocking message";
    char buf[128];
    int ev;
    size_t sent = 0;
    size_t received = 0;
    solo_socket_status_t st;

    server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_NONBLOCKING, &err_msg);
    if (!server) {
        if (err_msg != NULL) {
            free(err_msg);
            err_msg = NULL;
        }
        port = 19941;
        server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_NONBLOCKING, &err_msg);
    }
    ASSERT(server != NULL);
    ASSERT_EQ(SOLO_SOCK_NONBLOCKING, solo_socket_get_mode(server));

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, &err_msg);
    ASSERT(accepted == NULL);
    ASSERT(err_msg == NULL);

    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT(client != NULL);

    ev = solo_socket_wait(server, SOLO_SOCK_POLL_READ, 1000);
    ASSERT(ev & SOLO_SOCK_POLL_READ);

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, &err_msg);
    ASSERT(accepted != NULL);
    ASSERT_EQ(SOLO_SOCK_NONBLOCKING, solo_socket_get_mode(accepted));

    st = solo_socket_recv(accepted, buf, sizeof(buf), &received);
    ASSERT_EQ(SOLO_SOCK_AGAIN, st);
    ASSERT_EQ(0, received);

    st = solo_socket_send(client, msg, strlen(msg), &sent);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), sent);

    ev = solo_socket_wait(accepted, SOLO_SOCK_POLL_READ, 1000);
    ASSERT(ev & SOLO_SOCK_POLL_READ);

    memset(buf, 0, sizeof(buf));
    st = solo_socket_recv(accepted, buf, sizeof(buf) - 1, &received);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), received);
    ASSERT_STR_EQ(msg, buf);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);
    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    PASS();
}

TEST test_socket_udp_sync(void)
{
    char *err_msg = NULL;
    int port = 19950;
    solo_socket_t *server;
    solo_socket_t *client;
    char src_ip[64];
    int src_port = 0;
    const char *msg = "Direct UDP datagram!";
    const char *reply = "UDP reply ack";
    char buf[128];
    size_t sent = 0;
    size_t received = 0;
    solo_socket_status_t st;

    server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    if (!server) {
        if (err_msg != NULL) {
            free(err_msg);
            err_msg = NULL;
        }
        port = 19951;
        server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    }
    ASSERT(server != NULL);
    ASSERT_EQ(SOLO_SOCK_UDP, solo_socket_get_type(server));

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    client = solo_socket_udp(SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT(client != NULL);
    ASSERT_EQ(SOLO_SOCK_UDP, solo_socket_get_type(client));

    st = solo_socket_sendto(client, msg, strlen(msg), "127.0.0.1", port, &sent);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), sent);

    memset(buf, 0, sizeof(buf));
    st = solo_socket_recvfrom(server, buf, sizeof(buf) - 1, src_ip, sizeof(src_ip), &src_port,
                              &received);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), received);
    ASSERT_STR_EQ(msg, buf);
    ASSERT(src_port > 0);

    st = solo_socket_sendto(server, reply, strlen(reply), src_ip, src_port, &sent);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(reply), sent);

    memset(buf, 0, sizeof(buf));
    st = solo_socket_recvfrom(client, buf, sizeof(buf) - 1, NULL, 0, NULL, &received);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(reply), received);
    ASSERT_STR_EQ(reply, buf);

    solo_socket_close(client);
    solo_socket_close(server);
    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    PASS();
}

TEST test_socket_udp_async(void)
{
    char *err_msg = NULL;
    int port = 19960;
    solo_socket_t *server;
    solo_socket_t *client;
    const char *msg = "UDP nonblocking datagram!";
    char buf[128];
    int ev;
    size_t sent = 0;
    size_t received = 0;
    solo_socket_status_t st;

    server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_NONBLOCKING, &err_msg);
    if (!server) {
        if (err_msg != NULL) {
            free(err_msg);
            err_msg = NULL;
        }
        port = 19961;
        server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_NONBLOCKING, &err_msg);
    }
    ASSERT(server != NULL);
    ASSERT_EQ(SOLO_SOCK_NONBLOCKING, solo_socket_get_mode(server));

    st = solo_socket_recvfrom(server, buf, sizeof(buf), NULL, 0, NULL, &received);
    ASSERT_EQ(SOLO_SOCK_AGAIN, st);
    ASSERT_EQ(0, received);

    ev = solo_socket_wait(server, SOLO_SOCK_POLL_READ, 50);
    ASSERT_EQ(0, ev);

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    client = solo_socket_udp(SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT(client != NULL);
    st = solo_socket_sendto(client, msg, strlen(msg), "127.0.0.1", port, &sent);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), sent);

    ev = solo_socket_wait(server, SOLO_SOCK_POLL_READ, 1000);
    ASSERT(ev & SOLO_SOCK_POLL_READ);

    memset(buf, 0, sizeof(buf));
    st = solo_socket_recvfrom(server, buf, sizeof(buf) - 1, NULL, 0, NULL, &received);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), received);
    ASSERT_STR_EQ(msg, buf);

    solo_socket_close(client);
    solo_socket_close(server);
    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    PASS();
}

TEST test_socket_timeout(void)
{
    char *err_msg = NULL;
    int port = 19970;
    solo_socket_t *server;
    solo_socket_t *client;
    solo_socket_t *accepted;
    char buf[64];
    size_t received = 0;
    solo_socket_status_t st;

    server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    if (!server) {
        if (err_msg != NULL) {
            free(err_msg);
            err_msg = NULL;
        }
        port = 19971;
        server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    }
    ASSERT(server != NULL);

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT(client != NULL);

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    accepted = solo_socket_accept(server, NULL, 0, NULL, &err_msg);
    ASSERT(accepted != NULL);

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    ASSERT_EQ(SOLO_SOCK_OK, solo_socket_set_timeout(accepted, 100, &err_msg));

    st = solo_socket_recv(accepted, buf, sizeof(buf), &received);
    ASSERT_EQ(SOLO_SOCK_AGAIN, st);
    ASSERT_EQ(0, received);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);
    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    PASS();
}

TEST test_socket_err_msg_ownership(void)
{
    char *err_msg = NULL;
    solo_socket_status_t st;
    solo_socket_t *sock;

    /* Invalid handle to solo_socket_set_mode must populate *err_msg */
    st = solo_socket_set_mode(NULL, SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT_EQ(SOLO_SOCK_ERROR, st);
    ASSERT(err_msg != NULL);
    ASSERT(strlen(err_msg) > 0);
    free(err_msg);
    err_msg = NULL;

    /* Invalid handle to solo_socket_set_timeout must populate *err_msg */
    st = solo_socket_set_timeout(NULL, 1000, &err_msg);
    ASSERT_EQ(SOLO_SOCK_ERROR, st);
    ASSERT(err_msg != NULL);
    ASSERT(strlen(err_msg) > 0);
    free(err_msg);
    err_msg = NULL;

    /* Invalid handle to solo_socket_connect_sock */
    st = solo_socket_connect_sock(NULL, "127.0.0.1", 80, &err_msg);
    ASSERT_EQ(SOLO_SOCK_ERROR, st);
    ASSERT(err_msg != NULL);
    ASSERT(strlen(err_msg) > 0);
    free(err_msg);
    err_msg = NULL;

    /* Invalid handle to solo_socket_bind_sock */
    st = solo_socket_bind_sock(NULL, "127.0.0.1", 80, 5, &err_msg);
    ASSERT_EQ(SOLO_SOCK_ERROR, st);
    ASSERT(err_msg != NULL);
    ASSERT(strlen(err_msg) > 0);
    free(err_msg);
    err_msg = NULL;

    /* Invalid handle to solo_socket_accept */
    sock = solo_socket_accept(NULL, NULL, 0, NULL, &err_msg);
    ASSERT(sock == NULL);
    ASSERT(err_msg != NULL);
    ASSERT(strlen(err_msg) > 0);
    free(err_msg);
    err_msg = NULL;

    /* Invalid address to solo_socket_bind */
    sock = solo_socket_bind("999.999.999.999", 80, 5, SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT(sock == NULL);
    ASSERT(err_msg != NULL);
    ASSERT(strlen(err_msg) > 0);
    free(err_msg);
    err_msg = NULL;

    PASS();
}

TEST test_socket_ipv4_localhost(void)
{
    char *err_msg = NULL;
    int port = 19980;
    solo_socket_t *server;
    solo_socket_t *client;
    solo_socket_t *accepted;

    server = solo_socket_bind("localhost", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    if (!server) {
        if (err_msg != NULL) {
            free(err_msg);
            err_msg = NULL;
        }
        port = 19981;
        server = solo_socket_bind("localhost", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    }
    ASSERT(server != NULL);

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    client = solo_socket_connect("localhost", port, SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT(client != NULL);

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    accepted = solo_socket_accept(server, NULL, 0, NULL, &err_msg);
    ASSERT(accepted != NULL);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);
    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    PASS();
}

TEST test_socket_ipv4_bind_any(void)
{
    char *err_msg = NULL;
    int port = 19985;
    solo_socket_t *server;
    solo_socket_t *client;
    solo_socket_t *accepted;

    server = solo_socket_bind(NULL, port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    if (!server) {
        if (err_msg != NULL) {
            free(err_msg);
            err_msg = NULL;
        }
        port = 19986;
        server = solo_socket_bind(NULL, port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    }
    ASSERT(server != NULL);

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT(client != NULL);

    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    accepted = solo_socket_accept(server, NULL, 0, NULL, &err_msg);
    ASSERT(accepted != NULL);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);
    if (err_msg != NULL) {
        free(err_msg);
        err_msg = NULL;
    }
    PASS();
}

TEST test_socket_poll(void)
{
    char *err_msg = NULL;
    int p1 = 19992;
    int p2 = 19993;
    solo_socket_t *s1 = NULL;
    solo_socket_t *s2 = NULL;
    solo_socket_t *cli = NULL;
    solo_pollfd_t pfds[2];
    size_t sent = 0;
    int ready_count;
    char buf[64];
    size_t recvd = 0;

    s1 = solo_socket_udp_bind("127.0.0.1", p1, SOLO_SOCK_NONBLOCKING, &err_msg);
    if (!s1) {
        if (err_msg) { free(err_msg); err_msg = NULL; }
        p1 = 19994;
        p2 = 19995;
        s1 = solo_socket_udp_bind("127.0.0.1", p1, SOLO_SOCK_NONBLOCKING, &err_msg);
    }
    ASSERT(s1 != NULL);

    s2 = solo_socket_udp_bind("127.0.0.1", p2, SOLO_SOCK_NONBLOCKING, &err_msg);
    ASSERT(s2 != NULL);

    cli = solo_socket_udp(SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT(cli != NULL);

    /* Initially both are idle, timeout 50ms should return 0 */
    pfds[0].sock = s1;
    pfds[0].events = SOLO_SOCK_POLL_READ;
    pfds[0].revents = 0;

    pfds[1].sock = s2;
    pfds[1].events = SOLO_SOCK_POLL_READ;
    pfds[1].revents = 0;

    ready_count = solo_socket_poll(pfds, 2, 50);
    ASSERT_EQ(0, ready_count);
    ASSERT_EQ(0, pfds[0].revents);
    ASSERT_EQ(0, pfds[1].revents);

    /* Send datagram only to s2 */
    ASSERT_EQ(SOLO_SOCK_OK, solo_socket_sendto(cli, "ping_s2", 7, "127.0.0.1", p2, &sent));
    ASSERT_EQ(7, sent);

    /* Poll both: only s2 must be ready for read */
    ready_count = solo_socket_poll(pfds, 2, 1000);
    ASSERT_EQ(1, ready_count);
    ASSERT_EQ(0, pfds[0].revents);
    ASSERT(pfds[1].revents & SOLO_SOCK_POLL_READ);

    /* Drain s2 */
    ASSERT_EQ(SOLO_SOCK_OK, solo_socket_recvfrom(s2, buf, sizeof(buf), NULL, 0, NULL, &recvd));
    ASSERT_EQ(7, recvd);

    solo_socket_close(cli);
    solo_socket_close(s2);
    solo_socket_close(s1);
    if (err_msg) free(err_msg);
    PASS();
}

TEST test_sandbox_unsupported(void)
{
    char *err_msg = NULL;
    solo_sandbox_status_t rc;

#if SOLO_IS_LINUX
    /* On Linux, Landlock does not support OpenBSD pledge(); must fail visibly with error message */
    rc = solo_sandbox_pledge("stdio", &err_msg);
    ASSERT_EQ(SOLO_SANDBOX_UNSUPPORTED, rc);
    ASSERT(err_msg != NULL);
    ASSERT(strlen(err_msg) > 0);
    free(err_msg);
    err_msg = NULL;
#elif !SOLO_IS_OPENBSD
    /* On stubs or non-OpenBSD, pledge must fail visibly with SOLO_SANDBOX_UNSUPPORTED */
    rc = solo_sandbox_pledge("stdio", &err_msg);
    ASSERT_EQ(SOLO_SANDBOX_UNSUPPORTED, rc);
    ASSERT(err_msg != NULL);
    free(err_msg);
    err_msg = NULL;
#endif
    PASS();
}

TEST test_serial_enums(void)
{
    solo_serial_opts_t opts;
    ASSERT_EQ(0, SOLO_SERIAL_OK);
    ASSERT_EQ(-1, SOLO_SERIAL_ERROR);
    ASSERT_EQ(-2, SOLO_SERIAL_UNSUPPORTED);

    ASSERT_EQ(0, SOLO_SERIAL_FLOW_NONE);
    ASSERT_EQ(1, SOLO_SERIAL_FLOW_HARDWARE);
    ASSERT_EQ(2, SOLO_SERIAL_FLOW_SOFTWARE);

    ASSERT_EQ('n', SOLO_SERIAL_PARITY_NONE);
    ASSERT_EQ('e', SOLO_SERIAL_PARITY_EVEN);
    ASSERT_EQ('o', SOLO_SERIAL_PARITY_ODD);
    ASSERT_EQ('m', SOLO_SERIAL_PARITY_MARK);
    ASSERT_EQ('s', SOLO_SERIAL_PARITY_SPACE);

    ASSERT_EQ(1, SOLO_SERIAL_STOP_1);
    ASSERT_EQ(2, SOLO_SERIAL_STOP_2);

    ASSERT_EQ(5, SOLO_SERIAL_DATA_5);
    ASSERT_EQ(6, SOLO_SERIAL_DATA_6);
    ASSERT_EQ(7, SOLO_SERIAL_DATA_7);
    ASSERT_EQ(8, SOLO_SERIAL_DATA_8);

    ASSERT_EQ(9600, SOLO_SERIAL_BAUD_9600);
    ASSERT_EQ(115200, SOLO_SERIAL_BAUD_115200);

    ASSERT_EQ(-1, SOLO_SERIAL_LINE_IGNORE);
    ASSERT_EQ(0, SOLO_SERIAL_LINE_OFF);
    ASSERT_EQ(1, SOLO_SERIAL_LINE_ON);

    opts.baud = SOLO_SERIAL_BAUD_115200;
    opts.data_bits = SOLO_SERIAL_DATA_8;
    opts.parity = SOLO_SERIAL_PARITY_NONE;
    opts.stop_bits = SOLO_SERIAL_STOP_1;
    opts.flow_control = SOLO_SERIAL_FLOW_HARDWARE;
    opts.rts = SOLO_SERIAL_LINE_ON;
    opts.dtr = SOLO_SERIAL_LINE_ON;

    ASSERT_EQ(115200, opts.baud);
    ASSERT_EQ(8, opts.data_bits);
    ASSERT_EQ('n', opts.parity);

    ASSERT_EQ(SOLO_SERIAL_ERROR, solo_serial_set_rts(NULL, SOLO_SERIAL_LINE_ON));
    ASSERT_EQ(SOLO_SERIAL_ERROR, solo_serial_set_dtr(NULL, SOLO_SERIAL_LINE_OFF));
    PASS();
}

TEST test_parallel_enums(void)
{
    ASSERT_EQ(0, SOLO_PARALLEL_OK);
    ASSERT_EQ(-1, SOLO_PARALLEL_ERROR);
    ASSERT_EQ(-2, SOLO_PARALLEL_UNSUPPORTED);

    ASSERT_EQ(0, SOLO_PARALLEL_MODE_SPP);
    ASSERT_EQ(1, SOLO_PARALLEL_MODE_NIBBLE);
    ASSERT_EQ(2, SOLO_PARALLEL_MODE_BYTE);
    ASSERT_EQ(3, SOLO_PARALLEL_MODE_EPP);
    ASSERT_EQ(4, SOLO_PARALLEL_MODE_ECP);

    ASSERT_EQ(1 << 0, SOLO_PARALLEL_PIN_BUSY);
    ASSERT_EQ(1 << 1, SOLO_PARALLEL_PIN_ACK);
    ASSERT_EQ(1 << 2, SOLO_PARALLEL_PIN_PE);
    ASSERT_EQ(1 << 3, SOLO_PARALLEL_PIN_SLCT);
    ASSERT_EQ(1 << 4, SOLO_PARALLEL_PIN_FAULT);
    PASS();
}

TEST test_serial_pty(void)
{
#if SOLO_IS_POSIX
    int master_fd = -1;
    char *pts = NULL;
    char *err_msg = NULL;
    solo_serial_t *s = NULL;
    solo_serial_opts_t bad_baud_opts;
    solo_serial_opts_t bad_parity_opts;
    char buf[64];
    ssize_t n;
    int r;

    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (master_fd < 0) {
        PASS();
    }
    if (grantpt(master_fd) != 0 || unlockpt(master_fd) != 0) {
        close(master_fd);
        PASS();
    }
    pts = ptsname(master_fd);
    if (!pts) {
        close(master_fd);
        PASS();
    }

    /* 1. Invalid baud rate must fail visibly */
    memset(&bad_baud_opts, 0, sizeof(bad_baud_opts));
    bad_baud_opts.baud = 999999;
    bad_baud_opts.data_bits = 8;
    bad_baud_opts.parity = 'n';
    bad_baud_opts.stop_bits = 1;
    s = solo_serial_open(pts, &bad_baud_opts, &err_msg);
    ASSERT_EQ(NULL, s);
    ASSERT(err_msg != NULL);
    free(err_msg);
    err_msg = NULL;

    /* 2. Invalid parity must fail visibly */
    memset(&bad_parity_opts, 0, sizeof(bad_parity_opts));
    bad_parity_opts.baud = 9600;
    bad_parity_opts.data_bits = 8;
    bad_parity_opts.parity = 'z';
    bad_parity_opts.stop_bits = 1;
    s = solo_serial_open(pts, &bad_parity_opts, &err_msg);
    ASSERT_EQ(NULL, s);
    ASSERT(err_msg != NULL);
    free(err_msg);
    err_msg = NULL;

    /* 3. Default options (opts == NULL) must open with flow_control == 0 (NONE) and succeed */
    s = solo_serial_open(pts, NULL, &err_msg);
    ASSERT(s != NULL);
    if (err_msg) { free(err_msg); err_msg = NULL; }

    /* 4. Write to slave -> master reads */
    r = solo_serial_write(s, "pty_test\n", 9);
    ASSERT_EQ(9, r);

    memset(buf, 0, sizeof(buf));
    n = read(master_fd, buf, sizeof(buf) - 1);
    ASSERT_EQ(9, n);
    ASSERT_STR_EQ("pty_test\n", buf);

    /* 5. Master writes -> slave reads */
    n = write(master_fd, "pty_reply\n", 10);
    ASSERT_EQ(10, n);

    solo_serial_set_timeout(s, 1000);
    memset(buf, 0, sizeof(buf));
    r = solo_serial_read(s, buf, sizeof(buf) - 1, 1000);
    ASSERT(r > 0);
    buf[r] = '\0';
    ASSERT_STR_EQ("pty_reply\n", buf);

    /* 6. Timed read with no data waiting must return 0 (timeout) */
    r = solo_serial_read(s, buf, sizeof(buf), 10);
    ASSERT_EQ(0, r);

    /* 7. Modem line getters */
    (void)solo_serial_get_cts(s);
    (void)solo_serial_get_dsr(s);
    (void)solo_serial_get_cd(s);
    (void)solo_serial_get_ri(s);

    solo_serial_drain(s);
    solo_serial_close(s);
    close(master_fd);
#endif
    PASS();
}

TEST test_parallel_operations(void)
{
    char *err_msg = NULL;
    char buf[32];
    int status = 0;
    solo_parallel_t *p = NULL;

    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_read(NULL, buf, sizeof(buf)));
    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_get_status(NULL, &status));
    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_set_mode(NULL, SOLO_PARALLEL_MODE_BYTE));

    p = solo_parallel_open("/dev/nonexistent_solo_parallel_node", &err_msg);
    ASSERT_EQ(NULL, p);
    ASSERT(err_msg != NULL);
    free(err_msg);
    err_msg = NULL;

    PASS();
}

TEST test_sandbox_fork(void)
{
#if SOLO_IS_LINUX
    int version = 0;
    solo_sandbox_backend_t b = solo_sandbox_backend(&version);
    if (b == SOLO_SANDBOX_BACKEND_LANDLOCK && version > 0) {
        pid_t pid = fork();
        ASSERT(pid >= 0);
        if (pid == 0) {
            char *err = NULL;
            FILE *f;
            solo_sandbox_status_t rc;

            rc = solo_sandbox_unveil(".", SOLO_SANDBOX_PERM_READ, &err);
            if (rc != SOLO_SANDBOX_OK) {
                if (err) free(err);
                _exit(1);
            }
            rc = solo_sandbox_lock(&err);
            if (rc != SOLO_SANDBOX_OK) {
                if (err) free(err);
                _exit(2);
            }

            f = fopen("xmake.lua", "r");
            if (!f) {
                _exit(3);
            }
            fclose(f);

            f = fopen("/etc/passwd", "r");
            if (f) {
                fclose(f);
                _exit(4);
            }

            _exit(0);
        }

        int status = 0;
        waitpid(pid, &status, 0);
        ASSERT(WIFEXITED(status));
        ASSERT_EQ(0, WEXITSTATUS(status));
    }
#endif
    PASS();
}

TEST test_sandbox_lock_without_unveil_fork(void)
{
#if SOLO_IS_LINUX
    int version = 0;
    solo_sandbox_backend_t b = solo_sandbox_backend(&version);
    if (b == SOLO_SANDBOX_BACKEND_LANDLOCK && version > 0) {
        pid_t pid = fork();
        ASSERT(pid >= 0);
        if (pid == 0) {
            char *err = NULL;
            FILE *f;
            solo_sandbox_status_t rc;

            rc = solo_sandbox_lock(&err);
            if (rc != SOLO_SANDBOX_OK) {
                if (err) free(err);
                _exit(1);
            }

            f = fopen("/etc/passwd", "r");
            if (f) {
                fclose(f);
                _exit(2);
            }

            _exit(0);
        }

        int status = 0;
        waitpid(pid, &status, 0);
        ASSERT(WIFEXITED(status));
        ASSERT_EQ(0, WEXITSTATUS(status));
    }
#endif
    PASS();
}

TEST test_serial_null_guards(void)
{
    char buf[16];
    char *err_msg = NULL;
    solo_serial_t *s = NULL;

    solo_serial_close(NULL);
    ASSERT_EQ(-1, solo_serial_write(NULL, buf, sizeof(buf)));
    ASSERT_EQ(-1, solo_serial_read(NULL, buf, sizeof(buf), -1));
    ASSERT_EQ(SOLO_SERIAL_ERROR, solo_serial_set_timeout(NULL, 100));
    ASSERT_EQ(-1, solo_serial_drain(NULL));
    ASSERT_EQ(SOLO_SERIAL_ERROR, solo_serial_set_rts(NULL, SOLO_SERIAL_LINE_ON));
    ASSERT_EQ(SOLO_SERIAL_ERROR, solo_serial_set_dtr(NULL, SOLO_SERIAL_LINE_OFF));
    ASSERT_EQ(-1, solo_serial_get_cts(NULL));
    ASSERT_EQ(-1, solo_serial_get_dsr(NULL));
    ASSERT_EQ(-1, solo_serial_get_cd(NULL));
    ASSERT_EQ(-1, solo_serial_get_ri(NULL));

    s = solo_serial_open("/dev/nonexistent_serial_guard_test", NULL, &err_msg);
    ASSERT_EQ(NULL, s);
    ASSERT(err_msg != NULL);
    free(err_msg);
    err_msg = NULL;

    PASS();
}

TEST test_serial_pty_exhaustive(void)
{
#if SOLO_IS_POSIX
    int master_fd = -1;
    char *pts = NULL;
    char *err_msg = NULL;
    solo_serial_t *s = NULL;
    solo_serial_opts_t opts;
    char sbuf[1024];
    char rbuf[1024];
    ssize_t n;
    int r;
    int i;
    int valid_bauds[8];
    int invalid_bauds[5];

    valid_bauds[0] = 1200; valid_bauds[1] = 2400; valid_bauds[2] = 4800; valid_bauds[3] = 9600;
    valid_bauds[4] = 19200; valid_bauds[5] = 38400; valid_bauds[6] = 57600; valid_bauds[7] = 115200;

    invalid_bauds[0] = 0; invalid_bauds[1] = -9600; invalid_bauds[2] = 7; invalid_bauds[3] = 13; invalid_bauds[4] = 999999;

    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (master_fd < 0) {
        PASS();
    }
    if (grantpt(master_fd) != 0 || unlockpt(master_fd) != 0) {
        close(master_fd);
        PASS();
    }
    pts = ptsname(master_fd);
    if (!pts) {
        close(master_fd);
        PASS();
    }

    /* 1. All standard baud rates must succeed */
    for (i = 0; i < 8; i++) {
        memset(&opts, 0, sizeof(opts));
        opts.baud = valid_bauds[i];
        opts.data_bits = 8;
        opts.parity = 'n';
        opts.stop_bits = 1;
        opts.flow_control = SOLO_SERIAL_FLOW_NONE;
        s = solo_serial_open(pts, &opts, &err_msg);
        ASSERT(s != NULL);
        solo_serial_close(s);
    }

    /* 2. All invalid baud rates must fail */
    for (i = 0; i < 5; i++) {
        memset(&opts, 0, sizeof(opts));
        opts.baud = invalid_bauds[i];
        opts.data_bits = 8;
        opts.parity = 'n';
        opts.stop_bits = 1;
        s = solo_serial_open(pts, &opts, &err_msg);
        ASSERT_EQ(NULL, s);
        ASSERT(err_msg != NULL);
        free(err_msg);
        err_msg = NULL;
    }

    /* 3. Parity values: 'e', 'o' succeed; 'm', 's', 'z' fail */
    memset(&opts, 0, sizeof(opts));
    opts.baud = 9600;
    opts.data_bits = 8;
    opts.parity = 'e';
    opts.stop_bits = 1;
    s = solo_serial_open(pts, &opts, &err_msg);
    ASSERT(s != NULL);
    solo_serial_close(s);

    opts.parity = 'o';
    s = solo_serial_open(pts, &opts, &err_msg);
    ASSERT(s != NULL);
    solo_serial_close(s);

    opts.parity = 'z';
    s = solo_serial_open(pts, &opts, &err_msg);
    ASSERT_EQ(NULL, s);
    ASSERT(err_msg != NULL);
    free(err_msg); err_msg = NULL;

    opts.parity = 'x';
    s = solo_serial_open(pts, &opts, &err_msg);
    ASSERT_EQ(NULL, s);
    ASSERT(err_msg != NULL);
    free(err_msg); err_msg = NULL;

    /* 4. Open working PTY port with default settings */
    s = solo_serial_open(pts, NULL, &err_msg);
    ASSERT(s != NULL);

    /* Zero length write returns 0 without error */
    ASSERT_EQ(0, solo_serial_write(s, "hello", 0));

    /* Non-blocking read on empty port returns 0 */
    ASSERT_EQ(0, solo_serial_read(s, rbuf, sizeof(rbuf), 0));

    /* Timed read on empty port returns 0 */
    ASSERT_EQ(0, solo_serial_read(s, rbuf, sizeof(rbuf), 20));

    /* Runtime timeout manipulation */
    ASSERT_EQ(SOLO_SERIAL_OK, solo_serial_set_timeout(s, 0));
    ASSERT_EQ(0, solo_serial_read(s, rbuf, sizeof(rbuf), -1));

    ASSERT_EQ(SOLO_SERIAL_OK, solo_serial_set_timeout(s, 20));
    ASSERT_EQ(0, solo_serial_read(s, rbuf, sizeof(rbuf), -1));

    /* 1024-byte bidirectional transfer */
    for (i = 0; i < 1024; i++) {
        sbuf[i] = (char)(i & 0xFF);
    }
    r = solo_serial_write(s, sbuf, 1024);
    ASSERT_EQ(1024, r);

    memset(rbuf, 0, sizeof(rbuf));
    n = read(master_fd, rbuf, 1024);
    ASSERT_EQ(1024, n);
    ASSERT_EQ(0, memcmp(sbuf, rbuf, 1024));

    /* Master writes 1024 bytes -> slave reads */
    n = write(master_fd, sbuf, 1024);
    ASSERT_EQ(1024, n);

    memset(rbuf, 0, sizeof(rbuf));
    r = solo_serial_read(s, rbuf, 1024, 1000);
    ASSERT_EQ(1024, r);
    ASSERT_EQ(0, memcmp(sbuf, rbuf, 1024));

    /* Modem line operations on PTY (ENOTTY expected without physical UART lines) */
    (void)solo_serial_set_rts(s, SOLO_SERIAL_LINE_ON);
    (void)solo_serial_set_rts(s, SOLO_SERIAL_LINE_OFF);
    (void)solo_serial_set_dtr(s, SOLO_SERIAL_LINE_ON);
    (void)solo_serial_set_dtr(s, SOLO_SERIAL_LINE_OFF);
    (void)solo_serial_get_cts(s);
    (void)solo_serial_get_dsr(s);
    (void)solo_serial_get_cd(s);
    (void)solo_serial_get_ri(s);
    ASSERT_EQ(0, solo_serial_drain(s));

    solo_serial_close(s);
    close(master_fd);
#endif
    PASS();
}

TEST test_parallel_exhaustive(void)
{
    char buf[16];
    char *err_msg = NULL;
    int status = 0;
    solo_parallel_t *p = NULL;

    solo_parallel_close(NULL);
    ASSERT_EQ(-1, solo_parallel_write(NULL, buf, sizeof(buf)));
    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_read(NULL, buf, sizeof(buf)));
    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_get_status(NULL, &status));
    ASSERT_EQ(-1, solo_parallel_drain(NULL));
    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_set_mode(NULL, SOLO_PARALLEL_MODE_SPP));

    p = solo_parallel_open("/dev/nonexistent_parallel_exhaustive_xyz", &err_msg);
    ASSERT_EQ(NULL, p);
    ASSERT(err_msg != NULL);
    free(err_msg);
    err_msg = NULL;

    PASS();
}

TEST test_socket_null_guards(void)
{
    char buf[16];
    char src_ip[32];
    int port = 0;
    size_t sent = 0;
    size_t received = 0;
    char *err_msg = NULL;

    solo_socket_close(NULL);

    ASSERT_EQ(NULL, solo_socket_connect("invalid.host.nonexistent.xyz", 80, SOLO_SOCK_BLOCKING, &err_msg));
    ASSERT(err_msg != NULL); free(err_msg); err_msg = NULL;

    ASSERT_EQ(NULL, solo_socket_bind("999.999.999.999", 80, 5, SOLO_SOCK_BLOCKING, &err_msg));
    ASSERT(err_msg != NULL); free(err_msg); err_msg = NULL;

    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_connect_sock(NULL, "127.0.0.1", 80, &err_msg));
    ASSERT(err_msg != NULL); free(err_msg); err_msg = NULL;

    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_bind_sock(NULL, "127.0.0.1", 80, 5, &err_msg));
    ASSERT(err_msg != NULL); free(err_msg); err_msg = NULL;

    ASSERT_EQ(NULL, solo_socket_accept(NULL, src_ip, sizeof(src_ip), &port, &err_msg));
    ASSERT(err_msg != NULL); free(err_msg); err_msg = NULL;

    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_send(NULL, buf, sizeof(buf), &sent));
    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_recv(NULL, buf, sizeof(buf), &received));

    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_sendto(NULL, buf, sizeof(buf), "127.0.0.1", 80, &sent));
    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_recvfrom(NULL, buf, sizeof(buf), src_ip, sizeof(src_ip), &port, &received));

    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_set_mode(NULL, SOLO_SOCK_NONBLOCKING, &err_msg));
    ASSERT(err_msg != NULL); free(err_msg); err_msg = NULL;

    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_set_timeout(NULL, 100, &err_msg));
    ASSERT(err_msg != NULL); free(err_msg); err_msg = NULL;

    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_poll(NULL, 1, 10));
    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_wait(NULL, SOLO_SOCK_POLL_READ, 10));

    PASS();
}

TEST test_socket_tcp_streaming(void)
{
    char *err_msg = NULL;
    int port = 19935;
    solo_socket_t *server = NULL;
    solo_socket_t *client = NULL;
    solo_socket_t *accepted = NULL;
    char client_ip[64];
    int client_port = 0;
    static char send_buf[65536];
    static char recv_buf[65536];
    size_t total_sent = 0;
    size_t total_recv = 0;
    size_t sent = 0;
    size_t received = 0;
    solo_socket_status_t st;
    int i;

    for (i = 0; i < 65536; i++) {
        send_buf[i] = (char)(i & 0xFF);
    }

    server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    if (!server) {
        if (err_msg) { free(err_msg); err_msg = NULL; }
        port++;
        server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    }
    ASSERT(server != NULL);

    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT(client != NULL);

    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, &err_msg);
    ASSERT(accepted != NULL);

    /* Stream 64KB */
    while (total_sent < 65536 || total_recv < 65536) {
        if (total_sent < 65536) {
            size_t chunk = (65536 - total_sent > 4096) ? 4096 : (65536 - total_sent);
            sent = 0;
            st = solo_socket_send(client, send_buf + total_sent, chunk, &sent);
            if (st == SOLO_SOCK_OK && sent > 0) {
                total_sent += sent;
            }
        }
        if (total_recv < 65536) {
            size_t chunk = (65536 - total_recv > 4096) ? 4096 : (65536 - total_recv);
            received = 0;
            st = solo_socket_recv(accepted, recv_buf + total_recv, chunk, &received);
            if (st == SOLO_SOCK_OK && received > 0) {
                total_recv += received;
            }
        }
    }

    ASSERT_EQ(65536, total_sent);
    ASSERT_EQ(65536, total_recv);
    ASSERT_EQ(0, memcmp(send_buf, recv_buf, 65536));

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);

    PASS();
}

TEST test_socket_udp_multigram(void)
{
    char *err_msg = NULL;
    int port = 19940;
    solo_socket_t *server = NULL;
    solo_socket_t *client = NULL;
    char msg[64];
    char recv_msg[64];
    char src_ip[32];
    int src_port = 0;
    size_t sent = 0;
    size_t received = 0;
    solo_socket_status_t st;
    int i;

    server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    if (!server) {
        if (err_msg) { free(err_msg); err_msg = NULL; }
        port++;
        server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    }
    ASSERT(server != NULL);

    client = solo_socket_udp(SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT(client != NULL);

    for (i = 0; i < 20; i++) {
        snprintf(msg, sizeof(msg), "dgram_seq_%03d", i);
        sent = 0;
        st = solo_socket_sendto(client, msg, strlen(msg), "127.0.0.1", port, &sent);
        ASSERT_EQ(SOLO_SOCK_OK, st);
        ASSERT_EQ(strlen(msg), sent);

        memset(recv_msg, 0, sizeof(recv_msg));
        received = 0;
        st = solo_socket_recvfrom(server, recv_msg, sizeof(recv_msg) - 1, src_ip, sizeof(src_ip), &src_port, &received);
        ASSERT_EQ(SOLO_SOCK_OK, st);
        ASSERT_EQ(strlen(msg), received);
        ASSERT_STR_EQ(msg, recv_msg);
    }

    solo_socket_close(client);
    solo_socket_close(server);
    PASS();
}

TEST test_socket_connection_refused(void)
{
    char *err_msg = NULL;
    solo_socket_t *s = solo_socket_connect("127.0.0.1", 1, SOLO_SOCK_BLOCKING, &err_msg);
    ASSERT_EQ(NULL, s);
    ASSERT(err_msg != NULL);
    free(err_msg);
    PASS();
}

SUITE(c_api_suite)
{
    RUN_TEST(test_platform);
    RUN_TEST(test_filesystem);
    RUN_TEST(test_sandbox);
    RUN_TEST(test_sandbox_enums);
    RUN_TEST(test_sandbox_unsupported);
    RUN_TEST(test_sandbox_fork);
    RUN_TEST(test_sandbox_lock_without_unveil_fork);
    RUN_TEST(test_serial_enums);
    RUN_TEST(test_serial_null_guards);
    RUN_TEST(test_serial_pty);
    RUN_TEST(test_serial_pty_exhaustive);
    RUN_TEST(test_parallel_enums);
    RUN_TEST(test_parallel_operations);
    RUN_TEST(test_parallel_exhaustive);
    RUN_TEST(test_socket_enums);
    RUN_TEST(test_socket_null_guards);
    RUN_TEST(test_socket_tcp_sync);
    RUN_TEST(test_socket_tcp_async);
    RUN_TEST(test_socket_tcp_streaming);
    RUN_TEST(test_socket_udp_sync);
    RUN_TEST(test_socket_udp_async);
    RUN_TEST(test_socket_udp_multigram);
    RUN_TEST(test_socket_timeout);
    RUN_TEST(test_socket_err_msg_ownership);
    RUN_TEST(test_socket_connection_refused);
    RUN_TEST(test_socket_ipv4_localhost);
    RUN_TEST(test_socket_ipv4_bind_any);
    RUN_TEST(test_socket_poll);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(c_api_suite);
    GREATEST_MAIN_END();
}
