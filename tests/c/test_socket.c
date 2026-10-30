/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 * Mozilla Public License Version 2.0
 */

#include "test_common.h"

/*
 * ============================================================================
 * Network Sockets & Event Multiplexing Tests
 * ============================================================================
 * Solo provides a portable Berkeley sockets abstraction supporting:
 * - TCP stream sockets (blocking and non-blocking)
 * - UDP connectionless datagram sockets
 * - Synchronous and asynchronous loopback communication
 * - High-throughput streaming with flow control (64 KB transfers)
 * - Multiplexing via solo_socket_wait() and multi-socket solo_socket_poll()
 * - Configurable socket timeouts and safe NULL-pointer error handling
 */

/*
 * Verifies standard socket constants and bitmasks defined in solo/socket.h.
 */
TEST test_socket_enums(void)
{
    /* Socket protocol families */
    ASSERT_EQ(1, SOLO_SOCK_TCP);
    ASSERT_EQ(2, SOLO_SOCK_UDP);

    /* Socket blocking / non-blocking mode */
    ASSERT_EQ(0, SOLO_SOCK_BLOCKING);
    ASSERT_EQ(1, SOLO_SOCK_NONBLOCKING);

    /* Socket status return codes */
    ASSERT_EQ(0, SOLO_SOCK_OK);       /* Operation succeeded */
    ASSERT_EQ(-1, SOLO_SOCK_ERROR);   /* Fatal I/O or system error */
    ASSERT_EQ(-2, SOLO_SOCK_AGAIN);   /* Operation would block (EAGAIN/EWOULDBLOCK or timeout) */
    ASSERT_EQ(-3, SOLO_SOCK_CLOSED);  /* Remote peer gracefully closed the connection */

    /* Event polling masks for wait() and poll() */
    ASSERT_EQ(1 << 0, SOLO_SOCK_POLL_READ);   /* Socket has data ready for recv() / accept() */
    ASSERT_EQ(1 << 1, SOLO_SOCK_POLL_WRITE);  /* Socket can accept outgoing data without blocking */
    ASSERT_EQ(1 << 2, SOLO_SOCK_POLL_ERR);    /* Asynchronous socket error or hangup */
    PASS();
}

/*
 * Defensive API validation:
 * Verifies that all socket functions safely handle NULL pointers and invalid
 * input arguments without crashing (segfaulting), and correctly populate
 * caller-provided error buffers.
 */
TEST test_socket_null_guards(void)
{
    char buf[16];
    char src_ip[32];
    int port = 0;
    size_t sent = 0;
    size_t received = 0;
    char err_buf[128];

    /* Closing a NULL socket must be a safe no-op */
    solo_socket_close(NULL);

    /* Connecting to a malformed/nonexistent domain name fails with NULL */
    err_buf[0] = '\0';
    ASSERT_EQ(NULL, solo_socket_connect("invalid.host.nonexistent.xyz", 80, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf)));
    ASSERT(strlen(err_buf) > 0);

    /* Binding to an invalid IP address string fails with NULL */
    err_buf[0] = '\0';
    ASSERT_EQ(NULL, solo_socket_bind("999.999.999.999", 80, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf)));
    ASSERT(strlen(err_buf) > 0);

    /* Operations on unallocated/NULL solo_socket_t handles must fail safely */
    err_buf[0] = '\0';
    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_connect_sock(NULL, "127.0.0.1", 80, err_buf, sizeof(err_buf)));
    ASSERT(strlen(err_buf) > 0);

    err_buf[0] = '\0';
    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_bind_sock(NULL, "127.0.0.1", 80, 5, err_buf, sizeof(err_buf)));
    ASSERT(strlen(err_buf) > 0);

    err_buf[0] = '\0';
    ASSERT_EQ(NULL, solo_socket_accept(NULL, src_ip, sizeof(src_ip), &port, err_buf, sizeof(err_buf)));
    ASSERT(strlen(err_buf) > 0);

    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_send(NULL, buf, sizeof(buf), &sent));
    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_recv(NULL, buf, sizeof(buf), &received));

    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_sendto(NULL, buf, sizeof(buf), "127.0.0.1", 80, &sent));
    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_recvfrom(NULL, buf, sizeof(buf), src_ip, sizeof(src_ip), &port, &received));

    err_buf[0] = '\0';
    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_set_mode(NULL, SOLO_SOCK_NONBLOCKING, err_buf, sizeof(err_buf)));
    ASSERT(strlen(err_buf) > 0);

    err_buf[0] = '\0';
    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_set_timeout(NULL, 100, err_buf, sizeof(err_buf)));
    ASSERT(strlen(err_buf) > 0);

    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_poll(NULL, 1, 10));
    ASSERT_EQ(SOLO_SOCK_ERROR, solo_socket_wait(NULL, SOLO_SOCK_POLL_READ, 10));

    PASS();
}

/*
 * Synchronous TCP Client-Server Loopback Test:
 * 1. Binds and listens on loopback (127.0.0.1:port) in blocking mode.
 * 2. Connects a blocking TCP client to the server.
 * 3. Accepts the incoming connection, obtaining an 'accepted' client handle.
 * 4. Client sends a payload string to the server.
 * 5. Server reads the payload into a buffer and validates string equality.
 * 6. Gracefully closes all socket handles.
 */
TEST test_socket_tcp_sync(void)
{
    char err_buf[128];
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

    /* Step 1: Bind TCP server socket. Fallback to port+1 if port is busy */
    err_buf[0] = '\0';
    server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    if (!server) {
        port = 19931;
        err_buf[0] = '\0';
        server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    }
    ASSERT(server != NULL);
    ASSERT_EQ(SOLO_SOCK_TCP, solo_socket_get_type(server));
    ASSERT_EQ(SOLO_SOCK_BLOCKING, solo_socket_get_mode(server));

    /* Step 2: Connect TCP client socket */
    err_buf[0] = '\0';
    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT(client != NULL);
    ASSERT_EQ(SOLO_SOCK_TCP, solo_socket_get_type(client));

    /* Step 3: Accept client connection on server */
    err_buf[0] = '\0';
    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, err_buf, sizeof(err_buf));
    ASSERT(accepted != NULL);

    /* Step 4: Transmit data from client to server */
    st = solo_socket_send(client, msg, strlen(msg), &sent);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), sent);

    /* Step 5: Receive data on accepted server socket */
    memset(buf, 0, sizeof(buf));
    st = solo_socket_recv(accepted, buf, sizeof(buf) - 1, &received);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), received);
    ASSERT_STR_EQ(msg, buf);

    /* Step 6: Teardown */
    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);
    PASS();
}

/*
 * Asynchronous / Non-Blocking TCP Loopback Test:
 * 1. Binds server in non-blocking mode (SOLO_SOCK_NONBLOCKING).
 * 2. Confirms accept() returns NULL immediately when no client is connecting.
 * 3. Client connects. Server polls via solo_socket_wait() for incoming connection.
 * 4. Accepts connection; the accepted socket inherits non-blocking mode.
 * 5. Reading before any data is sent returns SOLO_SOCK_AGAIN (EAGAIN/EWOULDBLOCK).
 * 6. Client sends data. Server waits for POLL_READ readiness and receives the message.
 */
TEST test_socket_tcp_async(void)
{
    char err_buf[128];
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

    /* Bind server in non-blocking mode */
    err_buf[0] = '\0';
    server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_NONBLOCKING, err_buf, sizeof(err_buf));
    if (!server) {
        port = 19941;
        err_buf[0] = '\0';
        server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_NONBLOCKING, err_buf, sizeof(err_buf));
    }
    ASSERT(server != NULL);
    ASSERT_EQ(SOLO_SOCK_NONBLOCKING, solo_socket_get_mode(server));

    /* In non-blocking mode with no pending connections, accept() returns NULL without error */
    err_buf[0] = '\0';
    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, err_buf, sizeof(err_buf));
    ASSERT(accepted == NULL);

    /* Client initiates connection */
    err_buf[0] = '\0';
    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT(client != NULL);

    /* Poll server until incoming connection is ready to be accepted */
    ev = solo_socket_wait(server, SOLO_SOCK_POLL_READ, 1000);
    ASSERT(ev & SOLO_SOCK_POLL_READ);

    err_buf[0] = '\0';
    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, err_buf, sizeof(err_buf));
    ASSERT(accepted != NULL);
    ASSERT_EQ(SOLO_SOCK_NONBLOCKING, solo_socket_get_mode(accepted));

    /* Non-blocking read on empty buffer must return SOLO_SOCK_AGAIN */
    st = solo_socket_recv(accepted, buf, sizeof(buf), &received);
    ASSERT_EQ(SOLO_SOCK_AGAIN, st);
    ASSERT_EQ(0, received);

    /* Client sends payload */
    st = solo_socket_send(client, msg, strlen(msg), &sent);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), sent);

    /* Wait until the accepted socket has data ready */
    ev = solo_socket_wait(accepted, SOLO_SOCK_POLL_READ, 1000);
    ASSERT(ev & SOLO_SOCK_POLL_READ);

    /* Read and verify */
    memset(buf, 0, sizeof(buf));
    st = solo_socket_recv(accepted, buf, sizeof(buf) - 1, &received);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), received);
    ASSERT_STR_EQ(msg, buf);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);
    PASS();
}

/*
 * High-Throughput Streaming Stress Test:
 * Transmits a 64 KB (65,536 bytes) pseudo-random binary payload across TCP loopback
 * in 4 KB chunks, verifying stream integrity, TCP windowing, and byte-for-byte
 * identity with memcmp().
 */
TEST test_socket_tcp_streaming(void)
{
    char err_buf[128];
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

    /* Initialize 64 KB deterministic binary pattern */
    for (i = 0; i < 65536; i++) {
        send_buf[i] = (char)(i & 0xFF);
    }

    err_buf[0] = '\0';
    server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    if (!server) {
        port++;
        err_buf[0] = '\0';
        server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    }
    ASSERT(server != NULL);

    err_buf[0] = '\0';
    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT(client != NULL);

    err_buf[0] = '\0';
    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, err_buf, sizeof(err_buf));
    ASSERT(accepted != NULL);

    /* Stream 64 KB in chunks of up to 4 KB each, draining until complete */
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

    /* Assert that all 65,536 bytes were sent, received, and match identically */
    ASSERT_EQ(65536, total_sent);
    ASSERT_EQ(65536, total_recv);
    ASSERT_EQ(0, memcmp(send_buf, recv_buf, 65536));

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);

    PASS();
}

/*
 * Synchronous UDP Loopback Datagram Test:
 * 1. Binds UDP server to 127.0.0.1:port.
 * 2. Creates client UDP socket.
 * 3. Client sends datagram via solo_socket_sendto().
 * 4. Server receives datagram and sender address via solo_socket_recvfrom().
 * 5. Server sends reply back to client using discovered client port.
 * 6. Client receives reply and verifies content.
 */
TEST test_socket_udp_sync(void)
{
    char err_buf[128];
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

    /* Bind server UDP socket */
    err_buf[0] = '\0';
    server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    if (!server) {
        port = 19951;
        err_buf[0] = '\0';
        server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    }
    ASSERT(server != NULL);
    ASSERT_EQ(SOLO_SOCK_UDP, solo_socket_get_type(server));

    /* Create un-bound client UDP socket */
    err_buf[0] = '\0';
    client = solo_socket_udp(SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT(client != NULL);
    ASSERT_EQ(SOLO_SOCK_UDP, solo_socket_get_type(client));

    /* Send datagram from client to server */
    st = solo_socket_sendto(client, msg, strlen(msg), "127.0.0.1", port, &sent);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), sent);

    /* Receive datagram on server and inspect peer address info */
    memset(buf, 0, sizeof(buf));
    st = solo_socket_recvfrom(server, buf, sizeof(buf) - 1, src_ip, sizeof(src_ip), &src_port,
                              &received);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), received);
    ASSERT_STR_EQ(msg, buf);
    ASSERT(src_port > 0);

    /* Echo reply back to sender */
    st = solo_socket_sendto(server, reply, strlen(reply), src_ip, src_port, &sent);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(reply), sent);

    /* Client receives reply */
    memset(buf, 0, sizeof(buf));
    st = solo_socket_recvfrom(client, buf, sizeof(buf) - 1, NULL, 0, NULL, &received);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(reply), received);
    ASSERT_STR_EQ(reply, buf);

    solo_socket_close(client);
    solo_socket_close(server);
    PASS();
}

/*
 * Asynchronous / Non-Blocking UDP Datagram Test:
 * 1. Binds UDP server in non-blocking mode.
 * 2. Verifies recvfrom() on empty queue returns SOLO_SOCK_AGAIN immediately.
 * 3. Verifies solo_socket_wait() times out cleanly (returns 0) when idle.
 * 4. Client sends datagram.
 * 5. Server wakes up from wait() with POLL_READ and drains packet.
 */
TEST test_socket_udp_async(void)
{
    char err_buf[128];
    int port = 19960;
    solo_socket_t *server;
    solo_socket_t *client;
    const char *msg = "UDP nonblocking datagram!";
    char buf[128];
    int ev;
    size_t sent = 0;
    size_t received = 0;
    solo_socket_status_t st;

    err_buf[0] = '\0';
    server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_NONBLOCKING, err_buf, sizeof(err_buf));
    if (!server) {
        port = 19961;
        err_buf[0] = '\0';
        server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_NONBLOCKING, err_buf, sizeof(err_buf));
    }
    ASSERT(server != NULL);
    ASSERT_EQ(SOLO_SOCK_NONBLOCKING, solo_socket_get_mode(server));

    /* Non-blocking receive on empty socket returns SOLO_SOCK_AGAIN */
    st = solo_socket_recvfrom(server, buf, sizeof(buf), NULL, 0, NULL, &received);
    ASSERT_EQ(SOLO_SOCK_AGAIN, st);
    ASSERT_EQ(0, received);

    /* Wait with 50ms timeout expires with 0 */
    ev = solo_socket_wait(server, SOLO_SOCK_POLL_READ, 50);
    ASSERT_EQ(0, ev);

    err_buf[0] = '\0';
    client = solo_socket_udp(SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT(client != NULL);

    /* Send datagram */
    st = solo_socket_sendto(client, msg, strlen(msg), "127.0.0.1", port, &sent);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), sent);

    /* Server event wait triggers POLL_READ */
    ev = solo_socket_wait(server, SOLO_SOCK_POLL_READ, 1000);
    ASSERT(ev & SOLO_SOCK_POLL_READ);

    /* Read packet */
    memset(buf, 0, sizeof(buf));
    st = solo_socket_recvfrom(server, buf, sizeof(buf) - 1, NULL, 0, NULL, &received);
    ASSERT_EQ(SOLO_SOCK_OK, st);
    ASSERT_EQ(strlen(msg), received);
    ASSERT_STR_EQ(msg, buf);

    solo_socket_close(client);
    solo_socket_close(server);
    PASS();
}

/*
 * UDP Multigram Packet Boundary Preservation Test:
 * Sends a burst of 20 sequential UDP datagrams ("dgram_seq_000" to "dgram_seq_019").
 * Verifies that UDP message boundaries are preserved intact (unlike stream TCP).
 */
TEST test_socket_udp_multigram(void)
{
    char err_buf[128];
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

    err_buf[0] = '\0';
    server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    if (!server) {
        port++;
        err_buf[0] = '\0';
        server = solo_socket_udp_bind("127.0.0.1", port, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    }
    ASSERT(server != NULL);

    err_buf[0] = '\0';
    client = solo_socket_udp(SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT(client != NULL);

    /* Send and receive 20 individual datagrams in lockstep */
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

/*
 * Socket Timeout Behavior Test:
 * Sets a 100ms receive timeout via solo_socket_set_timeout().
 * Verifies that recv() on an idle connection returns SOLO_SOCK_AGAIN once the
 * timeout expires.
 */
TEST test_socket_timeout(void)
{
    char err_buf[128];
    int port = 19970;
    solo_socket_t *server;
    solo_socket_t *client;
    solo_socket_t *accepted;
    char buf[64];
    size_t received = 0;
    solo_socket_status_t st;

    err_buf[0] = '\0';
    server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    if (!server) {
        port = 19971;
        err_buf[0] = '\0';
        server = solo_socket_bind("127.0.0.1", port, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    }
    ASSERT(server != NULL);

    err_buf[0] = '\0';
    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT(client != NULL);

    err_buf[0] = '\0';
    accepted = solo_socket_accept(server, NULL, 0, NULL, err_buf, sizeof(err_buf));
    ASSERT(accepted != NULL);

    /* Configure 100ms timeout */
    err_buf[0] = '\0';
    ASSERT_EQ(SOLO_SOCK_OK, solo_socket_set_timeout(accepted, 100, err_buf, sizeof(err_buf)));

    /* Idle recv() must return SOLO_SOCK_AGAIN after ~100ms */
    st = solo_socket_recv(accepted, buf, sizeof(buf), &received);
    ASSERT_EQ(SOLO_SOCK_AGAIN, st);
    ASSERT_EQ(0, received);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);
    PASS();
}

/*
 * Caller-Provided Error Buffer Contract Test:
 * Verifies that functions accepting `(err_buf, err_cap)` populate the buffer
 * on failure with a non-empty descriptive error string without heap allocation.
 */
TEST test_socket_err_msg_ownership(void)
{
    char err_buf[128];
    solo_socket_status_t st;
    solo_socket_t *sock;

    /* 1. Invalid handle to solo_socket_set_mode */
    err_buf[0] = '\0';
    st = solo_socket_set_mode(NULL, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT_EQ(SOLO_SOCK_ERROR, st);
    ASSERT(strlen(err_buf) > 0);

    /* 2. Invalid handle to solo_socket_set_timeout */
    err_buf[0] = '\0';
    st = solo_socket_set_timeout(NULL, 1000, err_buf, sizeof(err_buf));
    ASSERT_EQ(SOLO_SOCK_ERROR, st);
    ASSERT(strlen(err_buf) > 0);

    /* 3. Invalid handle to solo_socket_connect_sock */
    err_buf[0] = '\0';
    st = solo_socket_connect_sock(NULL, "127.0.0.1", 80, err_buf, sizeof(err_buf));
    ASSERT_EQ(SOLO_SOCK_ERROR, st);
    ASSERT(strlen(err_buf) > 0);

    /* 4. Invalid handle to solo_socket_bind_sock */
    err_buf[0] = '\0';
    st = solo_socket_bind_sock(NULL, "127.0.0.1", 80, 5, err_buf, sizeof(err_buf));
    ASSERT_EQ(SOLO_SOCK_ERROR, st);
    ASSERT(strlen(err_buf) > 0);

    /* 5. Invalid handle to solo_socket_accept */
    err_buf[0] = '\0';
    sock = solo_socket_accept(NULL, NULL, 0, NULL, err_buf, sizeof(err_buf));
    ASSERT(sock == NULL);
    ASSERT(strlen(err_buf) > 0);

    /* 6. Invalid address to solo_socket_bind */
    err_buf[0] = '\0';
    sock = solo_socket_bind("999.999.999.999", 80, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT(sock == NULL);
    ASSERT(strlen(err_buf) > 0);

    PASS();
}

/*
 * Connection Refusal Test:
 * Connecting to an inactive port (e.g. 127.0.0.1:1) must fail immediately with
 * NULL and return a descriptive error message without hanging or crashing.
 */
TEST test_socket_connection_refused(void)
{
    char err_buf[128];
    solo_socket_t *s;
    err_buf[0] = '\0';
    s = solo_socket_connect("127.0.0.1", 1, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT_EQ(NULL, s);
    ASSERT(strlen(err_buf) > 0);
    PASS();
}

/*
 * Named Localhost Hostname Resolution Test:
 * Verifies that binding and connecting via the standard string "localhost"
 * correctly resolves to 127.0.0.1 and establishes a communication channel.
 */
TEST test_socket_ipv4_localhost(void)
{
    char err_buf[128];
    int port = 19980;
    solo_socket_t *server;
    solo_socket_t *client;
    solo_socket_t *accepted;

    err_buf[0] = '\0';
    server = solo_socket_bind("localhost", port, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    if (!server) {
        port = 19981;
        err_buf[0] = '\0';
        server = solo_socket_bind("localhost", port, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    }
    ASSERT(server != NULL);

    err_buf[0] = '\0';
    client = solo_socket_connect("localhost", port, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT(client != NULL);

    err_buf[0] = '\0';
    accepted = solo_socket_accept(server, NULL, 0, NULL, err_buf, sizeof(err_buf));
    ASSERT(accepted != NULL);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);
    PASS();
}

/*
 * INADDR_ANY (Bind Any) Socket Test:
 * Passing NULL as the bind address binds to INADDR_ANY (0.0.0.0), allowing the
 * server to accept connections on all available network interfaces.
 */
TEST test_socket_ipv4_bind_any(void)
{
    char err_buf[128];
    int port = 19985;
    solo_socket_t *server;
    solo_socket_t *client;
    solo_socket_t *accepted;

    /* Passing address NULL binds to INADDR_ANY (0.0.0.0) */
    err_buf[0] = '\0';
    server = solo_socket_bind(NULL, port, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    if (!server) {
        port = 19986;
        err_buf[0] = '\0';
        server = solo_socket_bind(NULL, port, 5, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    }
    ASSERT(server != NULL);

    err_buf[0] = '\0';
    client = solo_socket_connect("127.0.0.1", port, SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT(client != NULL);

    err_buf[0] = '\0';
    accepted = solo_socket_accept(server, NULL, 0, NULL, err_buf, sizeof(err_buf));
    ASSERT(accepted != NULL);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);
    PASS();
}

/*
 * Multi-Socket Event Multiplexing Test (solo_socket_poll):
 * 1. Creates two separate non-blocking UDP servers (s1, s2).
 * 2. Creates a client UDP socket.
 * 3. Populates solo_pollfd_t array observing POLL_READ on both s1 and s2.
 * 4. Polling while idle returns 0 ready descriptors.
 * 5. Client sends a datagram targeting ONLY s2.
 * 6. Polling both sockets returns 1 ready descriptor: s1 revents == 0, s2 revents has POLL_READ.
 * 7. Drains s2 and completes cleanup.
 */
TEST test_socket_poll(void)
{
    char err_buf[128];
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

    /* Bind s1 */
    err_buf[0] = '\0';
    s1 = solo_socket_udp_bind("127.0.0.1", p1, SOLO_SOCK_NONBLOCKING, err_buf, sizeof(err_buf));
    if (!s1) {
        p1 = 19994;
        p2 = 19995;
        err_buf[0] = '\0';
        s1 = solo_socket_udp_bind("127.0.0.1", p1, SOLO_SOCK_NONBLOCKING, err_buf, sizeof(err_buf));
    }
    ASSERT(s1 != NULL);

    /* Bind s2 */
    err_buf[0] = '\0';
    s2 = solo_socket_udp_bind("127.0.0.1", p2, SOLO_SOCK_NONBLOCKING, err_buf, sizeof(err_buf));
    ASSERT(s2 != NULL);

    err_buf[0] = '\0';
    cli = solo_socket_udp(SOLO_SOCK_BLOCKING, err_buf, sizeof(err_buf));
    ASSERT(cli != NULL);

    /* Setup pollfd array for both sockets */
    pfds[0].sock = s1;
    pfds[0].events = SOLO_SOCK_POLL_READ;
    pfds[0].revents = 0;

    pfds[1].sock = s2;
    pfds[1].events = SOLO_SOCK_POLL_READ;
    pfds[1].revents = 0;

    /* Idle poll: timeout 50ms should return 0 ready sockets */
    ready_count = solo_socket_poll(pfds, 2, 50);
    ASSERT_EQ(0, ready_count);
    ASSERT_EQ(0, pfds[0].revents);
    ASSERT_EQ(0, pfds[1].revents);

    /* Send datagram directed ONLY to s2 */
    ASSERT_EQ(SOLO_SOCK_OK, solo_socket_sendto(cli, "ping_s2", 7, "127.0.0.1", p2, &sent));
    ASSERT_EQ(7, sent);

    /* Poll both sockets: only s2 must be marked ready for reading */
    ready_count = solo_socket_poll(pfds, 2, 1000);
    ASSERT_EQ(1, ready_count);
    ASSERT_EQ(0, pfds[0].revents);
    ASSERT(pfds[1].revents & SOLO_SOCK_POLL_READ);

    /* Drain s2 and verify payload */
    ASSERT_EQ(SOLO_SOCK_OK, solo_socket_recvfrom(s2, buf, sizeof(buf), NULL, 0, NULL, &recvd));
    ASSERT_EQ(7, recvd);

    solo_socket_close(cli);
    solo_socket_close(s2);
    solo_socket_close(s1);
    PASS();
}

SUITE(socket_suite)
{
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
