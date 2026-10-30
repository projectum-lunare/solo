/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 * Mozilla Public License Version 2.0
 */

#include "test_common.h"

/*
 * ============================================================================
 * IEEE 1284 / SPP Parallel Port Driver Tests
 * ============================================================================
 * Tests the parallel port abstraction layer across operating modes (SPP,
 * Nibble, Byte, EPP, ECP), status register pin bitmasks (BUSY, ACK, PE, SLCT,
 * FAULT), defensive NULL-pointer handling, and device node open semantics.
 */

/*
 * Verifies standard constant values and bitmasks defined in solo/parallel.h.
 */
TEST test_parallel_enums(void)
{
    /* Status return codes */
    ASSERT_EQ(0, SOLO_PARALLEL_OK);
    ASSERT_EQ(-1, SOLO_PARALLEL_ERROR);
    ASSERT_EQ(-2, SOLO_PARALLEL_UNSUPPORTED);

    /* Port operating modes (SPP standard unidirectional, and IEEE 1284 bidirectional modes) */
    ASSERT_EQ(0, SOLO_PARALLEL_MODE_SPP);
    ASSERT_EQ(1, SOLO_PARALLEL_MODE_NIBBLE);
    ASSERT_EQ(2, SOLO_PARALLEL_MODE_BYTE);
    ASSERT_EQ(3, SOLO_PARALLEL_MODE_EPP);
    ASSERT_EQ(4, SOLO_PARALLEL_MODE_ECP);

    /* Hardware status register pin bitmasks */
    ASSERT_EQ(1 << 0, SOLO_PARALLEL_PIN_BUSY);   /* Printer busy line (inverted input on PC LPT) */
    ASSERT_EQ(1 << 1, SOLO_PARALLEL_PIN_ACK);    /* Acknowledge pulse from device */
    ASSERT_EQ(1 << 2, SOLO_PARALLEL_PIN_PE);     /* Paper End / Out of paper indicator */
    ASSERT_EQ(1 << 3, SOLO_PARALLEL_PIN_SLCT);   /* Select / Printer online status */
    ASSERT_EQ(1 << 4, SOLO_PARALLEL_PIN_FAULT);  /* Printer fault / error line */

    PASS();
}

/*
 * Validates error reporting and NULL handle safety during basic parallel operations:
 * - Calling read, get_status, or set_mode with NULL returns SOLO_PARALLEL_ERROR.
 * - Opening a nonexistent device node path fails gracefully with NULL and error message.
 */
TEST test_parallel_operations(void)
{
    char *err_msg = NULL;
    char buf[32];
    int status = 0;
    solo_parallel_t *p = NULL;

    /* Operations on NULL handle must fail safely */
    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_read(NULL, buf, sizeof(buf)));
    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_get_status(NULL, &status));
    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_set_mode(NULL, SOLO_PARALLEL_MODE_BYTE));

    /* Opening a nonexistent device path must fail with NULL and provide error text */
    p = solo_parallel_open("/dev/nonexistent_solo_parallel_node", &err_msg);
    ASSERT_EQ(NULL, p);
    ASSERT(err_msg != NULL);
    free(err_msg);
    err_msg = NULL;

    PASS();
}

/*
 * Exhaustive defensive safety test:
 * Verifies that all parallel port operations (close, write, read, get_status,
 * drain, set_mode) handle NULL handles without crashing, and that memory allocated
 * for error messages is properly tracked and freed.
 */
TEST test_parallel_exhaustive(void)
{
    char buf[16];
    char *err_msg = NULL;
    int status = 0;
    solo_parallel_t *p = NULL;

    /* Safe no-op on closing NULL */
    solo_parallel_close(NULL);

    /* Safe failure on write/read/drain/status/mode with NULL */
    ASSERT_EQ(-1, solo_parallel_write(NULL, buf, sizeof(buf)));
    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_read(NULL, buf, sizeof(buf)));
    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_get_status(NULL, &status));
    ASSERT_EQ(-1, solo_parallel_drain(NULL));
    ASSERT_EQ(SOLO_PARALLEL_ERROR, solo_parallel_set_mode(NULL, SOLO_PARALLEL_MODE_SPP));

    /* Opening an invalid device path returns NULL and populates err_msg */
    p = solo_parallel_open("/dev/nonexistent_parallel_exhaustive_xyz", &err_msg);
    ASSERT_EQ(NULL, p);
    ASSERT(err_msg != NULL);
    free(err_msg);
    err_msg = NULL;

    PASS();
}

SUITE(parallel_suite)
{
    RUN_TEST(test_parallel_enums);
    RUN_TEST(test_parallel_operations);
    RUN_TEST(test_parallel_exhaustive);
}
