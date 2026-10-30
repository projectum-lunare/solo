/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 * Mozilla Public License Version 2.0
 */

#include "test_common.h"

/*
 * ============================================================================
 * Solo C Unit Test Suite Entry Point
 * ============================================================================
 * This executable orchestrates and executes all modular test suites for the
 * Solo native C library using the Greatest testing framework.
 *
 * Each subsystem test suite is defined in its own compilation unit (.c) and
 * declared here via SUITE_EXTERN():
 * - platform_suite:   Host OS, architecture, word bitness, and endianness.
 * - filesystem_suite: Portable file and directory existence queries.
 * - sandbox_suite:    Landlock/Capsicum/Pledge isolation and permission controls.
 * - serial_suite:     UART/RS-232 serial driver, PTY loopback, baud rates.
 * - parallel_suite:   IEEE 1284 / SPP parallel port registers and status pins.
 * - socket_suite:     TCP/UDP networking, 64KB streaming, non-blocking I/O & polling.
 */

/* External test suite declarations */
SUITE_EXTERN(platform_suite);
SUITE_EXTERN(filesystem_suite);
SUITE_EXTERN(sandbox_suite);
SUITE_EXTERN(serial_suite);
SUITE_EXTERN(parallel_suite);
SUITE_EXTERN(socket_suite);

/* Greatest framework global definition macro (must appear only once across the binary) */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    /* Initialize greatest framework with command-line flags (e.g. -v for verbose) */
    GREATEST_MAIN_BEGIN();

    /* Sequentially execute modular test suites */
    RUN_SUITE(platform_suite);
    RUN_SUITE(filesystem_suite);
    RUN_SUITE(sandbox_suite);
    RUN_SUITE(serial_suite);
    RUN_SUITE(parallel_suite);
    RUN_SUITE(socket_suite);

    /* Print test summary (total tests, passed, failed, skipped, assertions) and exit */
    GREATEST_MAIN_END();
}
