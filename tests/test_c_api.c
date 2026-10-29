/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 * Mozilla Public License Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <greatest/greatest.h>

#include <solo/solo.h>

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

SUITE(c_api_suite)
{
    RUN_TEST(test_platform);
    RUN_TEST(test_filesystem);
    RUN_TEST(test_sandbox);
    RUN_TEST(test_sandbox_enums);
    RUN_TEST(test_sandbox_unsupported);
    RUN_TEST(test_serial_enums);
    RUN_TEST(test_parallel_enums);
    RUN_TEST(test_socket_enums);
    RUN_TEST(test_socket_tcp_sync);
    RUN_TEST(test_socket_tcp_async);
    RUN_TEST(test_socket_udp_sync);
    RUN_TEST(test_socket_udp_async);
    RUN_TEST(test_socket_timeout);
    RUN_TEST(test_socket_err_msg_ownership);
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
