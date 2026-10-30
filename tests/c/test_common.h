/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 * Mozilla Public License Version 2.0
 */

#ifndef SOLO_TEST_COMMON_H
#define SOLO_TEST_COMMON_H

/*
 * ============================================================================
 * Feature Test Macros & Platform Preprocessor Definitions
 * ============================================================================
 * _XOPEN_SOURCE 600:
 *   Exposes POSIX.1-2001 / SUSv3 APIs required for advanced pseudo-terminal (PTY)
 *   testing, such as posix_openpt(), grantpt(), unlockpt(), and ptsname().
 *
 * _DEFAULT_SOURCE 1:
 *   Enables BSD/GNU extension definitions (e.g. CMSPAR parity flags on Linux)
 *   without breaking standard POSIX compatibility.
 */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

/* Standard C89 headers required across all test suites */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Greatest testing framework: lightweight C89 unit testing harness */
#include <greatest/greatest.h>

/* Public solo API headers */
#include <solo/solo.h>

/* POSIX headers conditionally included for PTY and child-process isolation */
#if SOLO_IS_POSIX
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#endif /* SOLO_TEST_COMMON_H */
