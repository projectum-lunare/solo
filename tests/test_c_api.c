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

static void test_socket_loopback(void)
{
    char *err_msg = NULL;
    int port = 19930;
    solo_socket_t *server;
    solo_socket_t *client;
    solo_socket_t *accepted;
    char client_ip[64];
    int client_port = 0;
    const char *msg = "Direct socket test for solo!";
    char buf[128];
    int n;

    printf("  [TEST] socket: loopback TCP on 127.0.0.1:%d...\n", port);
    server = solo_socket_bind("127.0.0.1", port, 5, &err_msg);
    if (!server) {
        printf("  [WARN] Cannot bind port %d (%s), trying port 19931...\n", port, err_msg ? err_msg : "unknown");
        free(err_msg);
        err_msg = NULL;
        port = 19931;
        server = solo_socket_bind("127.0.0.1", port, 5, &err_msg);
    }
    assert(server != NULL);

    client = solo_socket_connect("127.0.0.1", port, &err_msg);
    assert(client != NULL);

    accepted = solo_socket_accept(server, client_ip, sizeof(client_ip), &client_port, &err_msg);
    assert(accepted != NULL);

    n = solo_socket_write(client, msg, strlen(msg));
    assert(n == (int)strlen(msg));

    memset(buf, 0, sizeof(buf));
    n = solo_socket_read(accepted, buf, sizeof(buf) - 1);
    assert(n == (int)strlen(msg));
    assert(strcmp(buf, msg) == 0);

    solo_socket_close(client);
    solo_socket_close(accepted);
    solo_socket_close(server);

    printf("  [PASS] socket loopback verified (%d bytes transferred).\n", n);
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
    test_socket_loopback();

    printf("\n>>> ALL C TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
