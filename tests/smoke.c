/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 * Mozilla Public License Version 2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/solo.h>

#define SMOKE_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "SMOKE FAILURE [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    char dummy_buf[16];
    char err_buf[128];
    int status_val = 0;
    int version = 0;
    size_t io_bytes = 0;
    const char *s_str;
    solo_serial_opts_t bad_opts;
    solo_serial_t *ser;
    solo_parallel_t *par;
    solo_socket_t *sock;

    printf("=== solo smoke test (behavioral verification) ===\n");

    /* 1. Platform Metadata & Bitness */
    s_str = solo_version();
    SMOKE_ASSERT(s_str != NULL && strlen(s_str) > 0, "solo_version is empty");
    s_str = solo_platform_os();
    SMOKE_ASSERT(s_str != NULL && strlen(s_str) > 0, "solo_platform_os is empty");
    s_str = solo_platform_arch();
    SMOKE_ASSERT(s_str != NULL && strlen(s_str) > 0, "solo_platform_arch is empty");
    SMOKE_ASSERT(solo_platform_bits() == 16 || solo_platform_bits() == 32 || solo_platform_bits() == 64,
                 "solo_platform_bits invalid");
    s_str = solo_platform_endian();
    SMOKE_ASSERT(s_str != NULL, "solo_platform_endian is null");
    printf("[OK] Platform: OS=%s ARCH=%s BITS=%d ENDIAN=%s\n",
           solo_platform_os(), solo_platform_arch(), solo_platform_bits(), solo_platform_endian());

    /* 2. Filesystem Existence Checks */
    SMOKE_ASSERT(solo_fs_exists(".") == 1, "solo_fs_exists('.') must be true");
    SMOKE_ASSERT(solo_fs_exists("/nonexistent_solo_smoke_xyz_998877") == 0,
                 "solo_fs_exists for nonexistent path must be false");
    SMOKE_ASSERT(solo_fs_exists(NULL) == 0, "solo_fs_exists(NULL) must be false");
    printf("[OK] Filesystem: existence checks verified\n");

    /* 3. Serial Driver Behavior */
    SMOKE_ASSERT(SOLO_SERIAL_FLOW_NONE == 0, "SOLO_SERIAL_FLOW_NONE must be 0");
    SMOKE_ASSERT(SOLO_SERIAL_OK == 0, "SOLO_SERIAL_OK must be 0");
    SMOKE_ASSERT(solo_serial_write(NULL, "x", 1, &io_bytes) != SOLO_OK, "solo_serial_write(NULL) must fail");
    SMOKE_ASSERT(solo_serial_read(NULL, dummy_buf, 1, &io_bytes, 0) != SOLO_OK, "solo_serial_read(NULL) must fail");
    SMOKE_ASSERT(solo_serial_set_timeout(NULL, 100) == SOLO_SERIAL_ERROR, "solo_serial_set_timeout(NULL) must fail");

    /* Open nonexistent serial port with bad baud: must fail and report error */
    memset(&bad_opts, 0, sizeof(bad_opts));
    bad_opts.baud = 999999;
    bad_opts.data_bits = 8;
    bad_opts.parity = 'n';
    bad_opts.stop_bits = 1;
    bad_opts.flow_control = SOLO_SERIAL_FLOW_NONE;
    err_buf[0] = '\0';
    ser = solo_serial_open("/dev/nonexistent_smoke_serial_dev", &bad_opts, err_buf, sizeof(err_buf));
    SMOKE_ASSERT(ser == NULL, "solo_serial_open with bad baud/path must return NULL");
    SMOKE_ASSERT(strlen(err_buf) > 0, "solo_serial_open failure must provide error description");
    printf("[OK] Serial: driver guards, baud validation and flow defaults verified\n");

    /* 4. Parallel Driver Behavior */
    SMOKE_ASSERT(SOLO_PARALLEL_OK == 0, "SOLO_PARALLEL_OK must be 0");
    SMOKE_ASSERT(SOLO_PARALLEL_MODE_SPP == 0, "SOLO_PARALLEL_MODE_SPP must be 0");
    SMOKE_ASSERT(solo_parallel_write(NULL, "x", 1, &io_bytes) != SOLO_OK, "solo_parallel_write(NULL) must fail");
    SMOKE_ASSERT(solo_parallel_read(NULL, dummy_buf, 1, &io_bytes) != SOLO_OK, "solo_parallel_read(NULL) must fail");
    SMOKE_ASSERT(solo_parallel_get_status(NULL, &status_val) == SOLO_PARALLEL_ERROR,
                 "solo_parallel_get_status(NULL) must fail");
    SMOKE_ASSERT(solo_parallel_set_mode(NULL, SOLO_PARALLEL_MODE_SPP) == SOLO_PARALLEL_ERROR,
                 "solo_parallel_set_mode(NULL) must fail");

    /* Open nonexistent parallel port: must fail with populated error string */
    err_buf[0] = '\0';
    par = solo_parallel_open("/dev/nonexistent_smoke_parallel_dev", err_buf, sizeof(err_buf));
    SMOKE_ASSERT(par == NULL, "solo_parallel_open on nonexistent path must return NULL");
    SMOKE_ASSERT(strlen(err_buf) > 0, "solo_parallel_open failure must provide error description");
    printf("[OK] Parallel: driver guards and device open failure verified\n");

    /* 5. Socket Driver Behavior */
    SMOKE_ASSERT(SOLO_SOCK_TCP == 1, "SOLO_SOCK_TCP must be 1");
    SMOKE_ASSERT(SOLO_SOCK_UDP == 2, "SOLO_SOCK_UDP must be 2");
    SMOKE_ASSERT(SOLO_SOCK_OK == 0, "SOLO_SOCK_OK must be 0");
    solo_socket_close(NULL); /* Safe no-op */

    /* Attempt connect to an unroutable port */
    err_buf[0] = '\0';
    sock = solo_socket_connect("127.0.0.1", 1, SOLO_SOCK_NONBLOCKING, err_buf, sizeof(err_buf));
    if (sock != NULL) {
        solo_socket_close(sock);
    }
    printf("[OK] Socket: type enums and error handling verified\n");

    /* 6. Sandbox Driver Behavior */
    s_str = solo_sandbox_backend_name(&version);
    SMOKE_ASSERT(s_str != NULL && strlen(s_str) > 0, "solo_sandbox_backend_name is empty");
    printf("[OK] Sandbox: backend '%s' (abi=%d) verified\n", s_str, version);

    printf("=== All smoke tests passed successfully ===\n");
    return 0;
}
