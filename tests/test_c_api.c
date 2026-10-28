/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 * Mozilla Public License Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <solo/solo.h>

static void test_platform(void)
{
    printf("  [TEST] platform: OS='%s', ARCH='%s', BITS=%d, ENDIAN='%s'...\n",
           solo_platform_os(), solo_platform_arch(), solo_platform_bits(), solo_platform_endian());
    assert(strcmp(solo_platform_os(), SOLO_OS_NAME) == 0);
    assert(strcmp(solo_platform_arch(), SOLO_ARCH_NAME) == 0);
    assert(solo_platform_bits() == SOLO_BITS);
    printf("  [PASS] platform verified.\n");
}

static void test_filesystem(void)
{
    printf("  [TEST] filesystem: checking existing and nonexistent paths...\n");
    assert(solo_fs_exists(".") == 1);
    assert(solo_fs_exists("/nonexistent_solo_xyz_12345") == 0);
    printf("  [PASS] filesystem verified.\n");
}

static void test_sandbox(void)
{
    int version = 0;
    const char *backend = solo_sandbox_backend_name(&version);
    printf("  [TEST] sandbox: backend='%s', version=%d...\n", backend, version);
    assert(backend != NULL);
    printf("  [PASS] sandbox verified.\n");
}

static void test_socket_enums(void)
{
    printf("  [TEST] socket: checking Enum definitions...\n");
    assert(SOLO_SOCK_TCP == 1);
    assert(SOLO_SOCK_UDP == 2);

    assert(SOLO_SOCK_BLOCKING == 0);
    assert(SOLO_SOCK_NONBLOCKING == 1);

    assert(SOLO_SOCK_OK == 0);
    assert(SOLO_SOCK_ERROR == -1);
    assert(SOLO_SOCK_AGAIN == -2);
    assert(SOLO_SOCK_CLOSED == -3);

    assert(SOLO_SOCK_POLL_READ == (1 << 0));
    assert(SOLO_SOCK_POLL_WRITE == (1 << 1));
    assert(SOLO_SOCK_POLL_ERR == (1 << 2));
    printf("  [PASS] socket Enums verified.\n");
}

static void test_socket_tcp_sync(void)
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

    printf("  [TEST] socket: loopback TCP sync on 127.0.0.1:%d...\n", port);
    server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    if (!server) {
        port = 19931;
        server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    }
    assert(server != NULL);
    assert(solo_socket_get_type(server) == SOLO_SOCK_TCP);
    assert(solo_socket_get_mode(server) == SOLO_SOCK_BLOCKING);

    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    assert(client != NULL);
    assert(solo_socket_get_type(client) == SOLO_SOCK_TCP);

    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, &err_msg);
    assert(accepted != NULL);

    st = solo_socket_send(client, msg, strlen(msg), &sent);
    assert(st == SOLO_SOCK_OK);
    assert(sent == strlen(msg));

    memset(buf, 0, sizeof(buf));
    st = solo_socket_recv(accepted, buf, sizeof(buf) - 1, &received);
    assert(st == SOLO_SOCK_OK);
    assert(received == strlen(msg));
    assert(strcmp(buf, msg) == 0);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);

    printf("  [PASS] socket TCP sync loopback verified (%d bytes transferred).\n", (int)received);
}

static void test_socket_tcp_async(void)
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

    printf("  [TEST] socket: loopback TCP async on 127.0.0.1:%d...\n", port);
    server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_NONBLOCKING, &err_msg);
    if (!server) {
        port = 19941;
        server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_NONBLOCKING, &err_msg);
    }
    assert(server != NULL);
    assert(solo_socket_get_mode(server) == SOLO_SOCK_NONBLOCKING);

    /* Non-blocking accept with no pending connection should return NULL without error */
    err_msg = NULL;
    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, &err_msg);
    assert(accepted == NULL);
    assert(err_msg == NULL);

    /* Connect client */
    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    assert(client != NULL);

    /* Server should become readable */
    ev = solo_socket_wait(server, SOLO_SOCK_POLL_READ, 1000);
    assert(ev & SOLO_SOCK_POLL_READ);

    /* Accept client */
    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, &err_msg);
    assert(accepted != NULL);

    /* Accepted socket inherits non-blocking from server */
    assert(solo_socket_get_mode(accepted) == SOLO_SOCK_NONBLOCKING);

    /* Read on empty accepted socket should return SOLO_SOCK_AGAIN */
    st = solo_socket_recv(accepted, buf, sizeof(buf), &received);
    assert(st == SOLO_SOCK_AGAIN);
    assert(received == 0);

    /* Write from client */
    st = solo_socket_send(client, msg, strlen(msg), &sent);
    assert(st == SOLO_SOCK_OK);
    assert(sent == strlen(msg));

    /* Wait for read readiness */
    ev = solo_socket_wait(accepted, SOLO_SOCK_POLL_READ, 1000);
    assert(ev & SOLO_SOCK_POLL_READ);

    /* Read data */
    memset(buf, 0, sizeof(buf));
    st = solo_socket_recv(accepted, buf, sizeof(buf) - 1, &received);
    assert(st == SOLO_SOCK_OK);
    assert(received == strlen(msg));
    assert(strcmp(buf, msg) == 0);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);

    printf("  [PASS] socket TCP async verified.\n");
}

static void test_socket_udp_sync(void)
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

    printf("  [TEST] socket: loopback UDP sync on 127.0.0.1:%d...\n", port);
    server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    if (!server) {
        port = 19951;
        server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    }
    assert(server != NULL);
    assert(solo_socket_get_type(server) == SOLO_SOCK_UDP);

    client = solo_socket_udp(SOLO_SOCK_BLOCKING, &err_msg);
    assert(client != NULL);
    assert(solo_socket_get_type(client) == SOLO_SOCK_UDP);

    /* Client sends datagram to server */
    st = solo_socket_sendto(client, msg, strlen(msg), "127.0.0.1", port, &sent);
    assert(st == SOLO_SOCK_OK);
    assert(sent == strlen(msg));

    /* Server receives datagram */
    memset(buf, 0, sizeof(buf));
    st = solo_socket_recvfrom(server, buf, sizeof(buf) - 1, src_ip, sizeof(src_ip), &src_port, &received);
    assert(st == SOLO_SOCK_OK);
    assert(received == strlen(msg));
    assert(strcmp(buf, msg) == 0);
    assert(src_port > 0);

    /* Server replies to client using src_ip and src_port */
    st = solo_socket_sendto(server, reply, strlen(reply), src_ip, src_port, &sent);
    assert(st == SOLO_SOCK_OK);
    assert(sent == strlen(reply));

    /* Client receives reply */
    memset(buf, 0, sizeof(buf));
    st = solo_socket_recvfrom(client, buf, sizeof(buf) - 1, NULL, 0, NULL, &received);
    assert(st == SOLO_SOCK_OK);
    assert(received == strlen(reply));
    assert(strcmp(buf, reply) == 0);

    solo_socket_close(client);
    solo_socket_close(server);

    printf("  [PASS] socket UDP sync loopback verified.\n");
}

static void test_socket_udp_async(void)
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

    printf("  [TEST] socket: loopback UDP async on 127.0.0.1:%d...\n", port);
    server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_NONBLOCKING, &err_msg);
    if (!server) {
        port = 19961;
        server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_NONBLOCKING, &err_msg);
    }
    assert(server != NULL);
    assert(solo_socket_get_mode(server) == SOLO_SOCK_NONBLOCKING);

    /* Recvfrom on empty UDP server should return SOLO_SOCK_AGAIN */
    st = solo_socket_recvfrom(server, buf, sizeof(buf), NULL, 0, NULL, &received);
    assert(st == SOLO_SOCK_AGAIN);
    assert(received == 0);

    /* Poll timeout check */
    ev = solo_socket_wait(server, SOLO_SOCK_POLL_READ, 50);
    assert(ev == 0); /* timeout */

    /* Send datagram from client */
    client = solo_socket_udp(SOLO_SOCK_BLOCKING, &err_msg);
    assert(client != NULL);
    st = solo_socket_sendto(client, msg, strlen(msg), "127.0.0.1", port, &sent);
    assert(st == SOLO_SOCK_OK);
    assert(sent == strlen(msg));

    /* Wait should now trigger read readiness */
    ev = solo_socket_wait(server, SOLO_SOCK_POLL_READ, 1000);
    assert(ev & SOLO_SOCK_POLL_READ);

    /* Recvfrom reads datagram */
    memset(buf, 0, sizeof(buf));
    st = solo_socket_recvfrom(server, buf, sizeof(buf) - 1, NULL, 0, NULL, &received);
    assert(st == SOLO_SOCK_OK);
    assert(received == strlen(msg));
    assert(strcmp(buf, msg) == 0);

    solo_socket_close(client);
    solo_socket_close(server);

    printf("  [PASS] socket UDP async verified.\n");
}

static void test_socket_timeout(void)
{
    char *err_msg = NULL;
    int port = 19970;
    solo_socket_t *server;
    solo_socket_t *client;
    solo_socket_t *accepted;
    char buf[64];
    size_t received = 0;
    solo_socket_status_t st;

    printf("  [TEST] socket: checking socket timeout...\n");
    server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    if (!server) {
        port = 19971;
        server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, &err_msg);
    }
    assert(server != NULL);

    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, &err_msg);
    assert(client != NULL);

    accepted = solo_socket_accept(server, NULL, 0, NULL, &err_msg);
    assert(accepted != NULL);

    /* Set 100ms receive timeout on accepted socket */
    assert(solo_socket_set_timeout(accepted, 100, &err_msg) == SOLO_SOCK_OK);

    /* Reading with no data sent should timeout and return SOLO_SOCK_AGAIN */
    st = solo_socket_recv(accepted, buf, sizeof(buf), &received);
    assert(st == SOLO_SOCK_AGAIN);
    assert(received == 0);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);

    printf("  [PASS] socket timeout verified.\n");
}

int main(void)
{
    printf("========================================\n");
    printf(" solo C API Unit Test Runner\n");
    printf(" Version: %s\n", solo_version());
    printf("========================================\n");

    test_platform();
    test_filesystem();
    test_sandbox();
    test_socket_enums();
    test_socket_tcp_sync();
    test_socket_tcp_async();
    test_socket_udp_sync();
    test_socket_udp_async();
    test_socket_timeout();

    printf("\n>>> ALL C TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
