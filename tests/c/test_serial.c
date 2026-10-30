/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 * Mozilla Public License Version 2.0
 */

#include "test_common.h"

/*
 * ============================================================================
 * Serial RS-232 / UART Driver Tests
 * ============================================================================
 * Tests serial communications across baud rates, parity, flow control, timeout
 * modes, and modem control lines.
 *
 * NOTE ON HARDWARE SIMULATION:
 * Physical serial ports (/dev/ttyS0, COM1) or USB-UART dongles are rarely
 * available on continuous integration or developer machines. On POSIX systems,
 * these tests simulate a full hardware UART loopback using POSIX pseudo-terminals
 * (PTY pairs via posix_openpt). The master side acts as the connected external
 * serial device, while the solo serial driver operates on the slave PTY device.
 */

/*
 * Verifies standard constant values, enum bitmasks, and default structure
 * fields defined in solo/serial.h.
 */
TEST test_serial_enums(void)
{
    solo_serial_opts_t opts;

    /* Status return codes */
    ASSERT_EQ(0, SOLO_SERIAL_OK);
    ASSERT_EQ(-1, SOLO_SERIAL_ERROR);
    ASSERT_EQ(-4, SOLO_SERIAL_UNSUPPORTED);

    /* Flow control constants */
    ASSERT_EQ(0, SOLO_SERIAL_FLOW_NONE);
    ASSERT_EQ(1, SOLO_SERIAL_FLOW_HARDWARE);
    ASSERT_EQ(2, SOLO_SERIAL_FLOW_SOFTWARE);

    /* Parity character constants */
    ASSERT_EQ('n', SOLO_SERIAL_PARITY_NONE);
    ASSERT_EQ('e', SOLO_SERIAL_PARITY_EVEN);
    ASSERT_EQ('o', SOLO_SERIAL_PARITY_ODD);
    ASSERT_EQ('m', SOLO_SERIAL_PARITY_MARK);
    ASSERT_EQ('s', SOLO_SERIAL_PARITY_SPACE);

    /* Stop bits */
    ASSERT_EQ(1, SOLO_SERIAL_STOP_1);
    ASSERT_EQ(2, SOLO_SERIAL_STOP_2);

    /* Data bits */
    ASSERT_EQ(5, SOLO_SERIAL_DATA_5);
    ASSERT_EQ(6, SOLO_SERIAL_DATA_6);
    ASSERT_EQ(7, SOLO_SERIAL_DATA_7);
    ASSERT_EQ(8, SOLO_SERIAL_DATA_8);

    /* Standard Baud rates */
    ASSERT_EQ(9600, SOLO_SERIAL_BAUD_9600);
    ASSERT_EQ(115200, SOLO_SERIAL_BAUD_115200);

    /* Modem line pin control values */
    ASSERT_EQ(-1, SOLO_SERIAL_LINE_IGNORE);
    ASSERT_EQ(0, SOLO_SERIAL_LINE_OFF);
    ASSERT_EQ(1, SOLO_SERIAL_LINE_ON);

    /* Verify configuration struct assignment */
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

    /* NULL handle guards on modem control functions */
    ASSERT_EQ(SOLO_SERIAL_ERROR, solo_serial_set_rts(NULL, SOLO_SERIAL_LINE_ON));
    ASSERT_EQ(SOLO_SERIAL_ERROR, solo_serial_set_dtr(NULL, SOLO_SERIAL_LINE_OFF));

    PASS();
}

/*
 * Defensive API validation:
 * Verifies that all serial functions safely reject NULL handle pointers,
 * returning predictable error codes instead of crashing (segfaulting).
 * Also verifies opening a nonexistent serial device returns NULL and an error message.
 */
TEST test_serial_null_guards(void)
{
    char buf[16];
    char err_buf[128];
    size_t io_bytes = 0;
    solo_serial_t *s = NULL;

    /* Closing NULL handle must be a safe no-op */
    solo_serial_close(NULL);

    /* Operations with NULL handle must return SOLO_ERROR */
    ASSERT_EQ(SOLO_ERROR, solo_serial_write(NULL, buf, sizeof(buf), &io_bytes));
    ASSERT_EQ(SOLO_ERROR, solo_serial_read(NULL, buf, sizeof(buf), &io_bytes, -1));
    ASSERT_EQ(SOLO_SERIAL_ERROR, solo_serial_set_timeout(NULL, 100));
    ASSERT_EQ(SOLO_SERIAL_ERROR, solo_serial_drain(NULL));
    ASSERT_EQ(SOLO_SERIAL_ERROR, solo_serial_set_rts(NULL, SOLO_SERIAL_LINE_ON));
    ASSERT_EQ(SOLO_SERIAL_ERROR, solo_serial_set_dtr(NULL, SOLO_SERIAL_LINE_OFF));
    ASSERT_EQ(-1, solo_serial_get_cts(NULL));
    ASSERT_EQ(-1, solo_serial_get_dsr(NULL));
    ASSERT_EQ(-1, solo_serial_get_cd(NULL));
    ASSERT_EQ(-1, solo_serial_get_ri(NULL));

    /* Opening nonexistent serial device path */
    err_buf[0] = '\0';
    s = solo_serial_open("/dev/nonexistent_serial_guard_test", NULL, err_buf, sizeof(err_buf));
    ASSERT_EQ(NULL, s);
    ASSERT(strlen(err_buf) > 0);

    PASS();
}

/*
 * Basic serial communication test using a POSIX Pseudo-Terminal (PTY) pair:
 * 1. Allocates a PTY master/slave pair (posix_openpt).
 * 2. Validates error handling on invalid baud and parity options.
 * 3. Opens the slave PTY device using solo_serial_open with default options.
 * 4. Transmits data from solo driver (slave) -> reads on master fd.
 * 5. Transmits data from master fd -> reads on solo driver (slave).
 * 6. Verifies timed read expiration on an idle line.
 */
TEST test_serial_pty(void)
{
#if SOLO_IS_POSIX
    int master_fd = -1;
    char *pts = NULL;
    char err_buf[128];
    solo_serial_t *s = NULL;
    solo_serial_opts_t bad_baud_opts;
    solo_serial_opts_t bad_parity_opts;
    char buf[64];
    ssize_t n;
    size_t written = 0;
    size_t read_bytes = 0;
    solo_status_t r;

    /* Allocate pseudo-terminal master */
    master_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (master_fd < 0) {
        /* If PTY creation fails in the environment (e.g. sandbox container), skip gracefully */
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

    /* 1. Invalid baud rate (999999) must fail visibly with an error message */
    memset(&bad_baud_opts, 0, sizeof(bad_baud_opts));
    bad_baud_opts.baud = 999999;
    bad_baud_opts.data_bits = 8;
    bad_baud_opts.parity = 'n';
    bad_baud_opts.stop_bits = 1;
    err_buf[0] = '\0';
    s = solo_serial_open(pts, &bad_baud_opts, err_buf, sizeof(err_buf));
    ASSERT_EQ(NULL, s);
    ASSERT(strlen(err_buf) > 0);

    /* 2. Invalid parity ('z') must fail visibly with an error message */
    memset(&bad_parity_opts, 0, sizeof(bad_parity_opts));
    bad_parity_opts.baud = 9600;
    bad_parity_opts.data_bits = 8;
    bad_parity_opts.parity = 'z';
    bad_parity_opts.stop_bits = 1;
    err_buf[0] = '\0';
    s = solo_serial_open(pts, &bad_parity_opts, err_buf, sizeof(err_buf));
    ASSERT_EQ(NULL, s);
    ASSERT(strlen(err_buf) > 0);

    /* 3. Opening with default options (opts == NULL) must default to flow NONE and succeed */
    err_buf[0] = '\0';
    s = solo_serial_open(pts, NULL, err_buf, sizeof(err_buf));
    ASSERT(s != NULL);

    /* 4. Write through solo driver -> read raw on master fd */
    r = solo_serial_write(s, "pty_test\n", 9, &written);
    ASSERT_EQ(SOLO_OK, r);
    ASSERT_EQ(9, (int)written);

    memset(buf, 0, sizeof(buf));
    n = read(master_fd, buf, sizeof(buf) - 1);
    ASSERT_EQ(9, n);
    ASSERT_STR_EQ("pty_test\n", buf);

    /* 5. Write raw on master fd -> read through solo driver with timeout */
    n = write(master_fd, "pty_reply\n", 10);
    ASSERT_EQ(10, n);

    solo_serial_set_timeout(s, 1000);
    memset(buf, 0, sizeof(buf));
    r = solo_serial_read(s, buf, sizeof(buf) - 1, &read_bytes, 1000);
    ASSERT_EQ(SOLO_OK, r);
    ASSERT(read_bytes > 0);
    buf[read_bytes] = '\0';
    ASSERT_STR_EQ("pty_reply\n", buf);

    /* 6. Timed read with no data waiting must return SOLO_AGAIN (timeout, not a fatal error) */
    r = solo_serial_read(s, buf, sizeof(buf), &read_bytes, 10);
    ASSERT_EQ(SOLO_AGAIN, r);

    /* 7. Modem line getters: safe execution on PTY device */
    (void)solo_serial_get_cts(s);
    (void)solo_serial_get_dsr(s);
    (void)solo_serial_get_cd(s);
    (void)solo_serial_get_ri(s);

    /* Flush/drain pending hardware buffers and clean up */
    solo_serial_drain(s);
    solo_serial_close(s);
    close(master_fd);
#endif

    PASS();
}

/*
 * Exhaustive serial driver stress test:
 * 1. Validates all 8 standard POSIX baud rates (1200 to 115200).
 * 2. Rejects 5 invalid/negative baud rates (0, -9600, 7, 13, 999999).
 * 3. Tests even ('e') and odd ('o') parity modes, and rejects invalid options ('z', 'x').
 * 4. Verifies zero-length write returns 0 without side effects.
 * 5. Verifies non-blocking (0ms) and timed reads return SOLO_AGAIN on empty input.
 * 6. Tests dynamic runtime reconfiguration of timeouts via solo_serial_set_timeout().
 * 7. Performs a full 1024-byte bidirectional binary transfer with memcmp verification.
 * 8. Exercises modem control line commands and hardware buffer drainage.
 */
TEST test_serial_pty_exhaustive(void)
{
#if SOLO_IS_POSIX
    int master_fd = -1;
    char *pts = NULL;
    char err_buf[128];
    solo_serial_t *s = NULL;
    solo_serial_opts_t opts;
    char sbuf[1024];
    char rbuf[1024];
    ssize_t n;
    size_t written = 0;
    size_t read_bytes = 0;
    solo_status_t r;
    int i;
    int valid_bauds[8];
    int invalid_bauds[5];

    valid_bauds[0] = 1200;  valid_bauds[1] = 2400;  valid_bauds[2] = 4800;  valid_bauds[3] = 9600;
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
        err_buf[0] = '\0';
        s = solo_serial_open(pts, &opts, err_buf, sizeof(err_buf));
        ASSERT(s != NULL);
        solo_serial_close(s);
    }

    /* 2. All invalid baud rates must fail and generate descriptive error messages */
    for (i = 0; i < 5; i++) {
        memset(&opts, 0, sizeof(opts));
        opts.baud = invalid_bauds[i];
        opts.data_bits = 8;
        opts.parity = 'n';
        opts.stop_bits = 1;
        err_buf[0] = '\0';
        s = solo_serial_open(pts, &opts, err_buf, sizeof(err_buf));
        ASSERT_EQ(NULL, s);
        ASSERT(strlen(err_buf) > 0);
    }

    /* 3. Parity configuration: standard 'e' and 'o' succeed; invalid 'z' and 'x' fail */
    memset(&opts, 0, sizeof(opts));
    opts.baud = 9600;
    opts.data_bits = 8;
    opts.parity = 'e';
    opts.stop_bits = 1;
    err_buf[0] = '\0';
    s = solo_serial_open(pts, &opts, err_buf, sizeof(err_buf));
    ASSERT(s != NULL);
    solo_serial_close(s);

    opts.parity = 'o';
    err_buf[0] = '\0';
    s = solo_serial_open(pts, &opts, err_buf, sizeof(err_buf));
    ASSERT(s != NULL);
    solo_serial_close(s);

    opts.parity = 'z';
    err_buf[0] = '\0';
    s = solo_serial_open(pts, &opts, err_buf, sizeof(err_buf));
    ASSERT_EQ(NULL, s);
    ASSERT(strlen(err_buf) > 0);

    opts.parity = 'x';
    err_buf[0] = '\0';
    s = solo_serial_open(pts, &opts, err_buf, sizeof(err_buf));
    ASSERT_EQ(NULL, s);
    ASSERT(strlen(err_buf) > 0);

    /* 4. Open working PTY port with default configuration */
    err_buf[0] = '\0';
    s = solo_serial_open(pts, NULL, err_buf, sizeof(err_buf));
    ASSERT(s != NULL);

    /* Zero-length write returns SOLO_OK without error */
    written = 999;
    ASSERT_EQ(SOLO_OK, solo_serial_write(s, "hello", 0, &written));
    ASSERT_EQ(0, (int)written);

    /* Non-blocking read (timeout_ms = 0) on empty queue returns SOLO_AGAIN immediately */
    read_bytes = 999;
    ASSERT_EQ(SOLO_AGAIN, solo_serial_read(s, rbuf, sizeof(rbuf), &read_bytes, 0));
    ASSERT_EQ(0, (int)read_bytes);

    /* Timed read (timeout_ms = 20) on empty queue returns SOLO_AGAIN upon expiration */
    read_bytes = 999;
    ASSERT_EQ(SOLO_AGAIN, solo_serial_read(s, rbuf, sizeof(rbuf), &read_bytes, 20));
    ASSERT_EQ(0, (int)read_bytes);

    /* Runtime timeout manipulation via solo_serial_set_timeout() */
    ASSERT_EQ(SOLO_SERIAL_OK, solo_serial_set_timeout(s, 0));
    read_bytes = 999;
    ASSERT_EQ(SOLO_AGAIN, solo_serial_read(s, rbuf, sizeof(rbuf), &read_bytes, -1)); /* -1 uses installed port timeout */
    ASSERT_EQ(0, (int)read_bytes);

    ASSERT_EQ(SOLO_SERIAL_OK, solo_serial_set_timeout(s, 20));
    read_bytes = 999;
    ASSERT_EQ(SOLO_AGAIN, solo_serial_read(s, rbuf, sizeof(rbuf), &read_bytes, -1));
    ASSERT_EQ(0, (int)read_bytes);

    /* 5. Bidirectional 1024-byte full buffer binary transfer */
    for (i = 0; i < 1024; i++) {
        sbuf[i] = (char)(i & 0xFF);
    }
    written = 0;
    r = solo_serial_write(s, sbuf, 1024, &written);
    ASSERT_EQ(SOLO_OK, r);
    ASSERT_EQ(1024, (int)written);

    memset(rbuf, 0, sizeof(rbuf));
    n = read(master_fd, rbuf, 1024);
    ASSERT_EQ(1024, n);
    ASSERT_EQ(0, memcmp(sbuf, rbuf, 1024));

    /* Master writes 1024 bytes -> solo serial driver reads and compares byte-by-byte */
    n = write(master_fd, sbuf, 1024);
    ASSERT_EQ(1024, n);

    memset(rbuf, 0, sizeof(rbuf));
    read_bytes = 0;
    r = solo_serial_read(s, rbuf, 1024, &read_bytes, 1000);
    ASSERT_EQ(SOLO_OK, r);
    ASSERT_EQ(1024, (int)read_bytes);
    ASSERT_EQ(0, memcmp(sbuf, rbuf, 1024));

    /* 6. Modem line operations on PTY (ENOTTY expected without physical UART lines) */
    (void)solo_serial_set_rts(s, SOLO_SERIAL_LINE_ON);
    (void)solo_serial_set_rts(s, SOLO_SERIAL_LINE_OFF);
    (void)solo_serial_set_dtr(s, SOLO_SERIAL_LINE_ON);
    (void)solo_serial_set_dtr(s, SOLO_SERIAL_LINE_OFF);
    (void)solo_serial_get_cts(s);
    (void)solo_serial_get_dsr(s);
    (void)solo_serial_get_cd(s);
    (void)solo_serial_get_ri(s);

    /* Hardware output drain */
    ASSERT_EQ(SOLO_OK, solo_serial_drain(s));

    solo_serial_close(s);
    close(master_fd);
#endif

    PASS();
}

SUITE(serial_suite)
{
    RUN_TEST(test_serial_enums);
    RUN_TEST(test_serial_null_guards);
    RUN_TEST(test_serial_pty);
    RUN_TEST(test_serial_pty_exhaustive);
}
