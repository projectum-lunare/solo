/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_TYPES_H
#define SOLO_TYPES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * =============================================================================
 * Canonical Boolean Type & Values
 * =============================================================================
 * Strict C89/C90 compatible boolean type with seamless C99 <stdbool.h> and C++
 * interoperability. Guarantees 0 is SOLO_FALSE and 1 is SOLO_TRUE across all
 * architectures and compilers.
 */
#if defined(__cplusplus)
    typedef bool solo_bool_t;
    #ifndef SOLO_FALSE
    #define SOLO_FALSE false
    #define SOLO_TRUE  true
    #endif
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
    #include <stdbool.h>
    typedef bool solo_bool_t;
    #ifndef SOLO_FALSE
    #define SOLO_FALSE false
    #define SOLO_TRUE  true
    #endif
#else
    typedef enum solo_bool {
        SOLO_FALSE = 0,
        SOLO_TRUE  = 1
    } solo_bool_t;
    #ifndef SOLO_FALSE
    #define SOLO_FALSE SOLO_FALSE
    #define SOLO_TRUE  SOLO_TRUE
    #endif
#endif

/*
 * =============================================================================
 * Canonical Hardware Logic Level / Signal Line State Enum
 * =============================================================================
 * Expresses digital logic levels and physical signal line states across
 * serial, parallel, and GPIO interfaces without ambiguous tri-state integers.
 */
typedef enum solo_line_state {
    SOLO_LINE_ERROR = -1, /* Signal query failed or unsupported on hardware */
    SOLO_LINE_LOW   =  0, /* Deasserted / inactive / logical low (0) */
    SOLO_LINE_HIGH  =  1  /* Asserted / active / logical high (1) */
} solo_line_state_t;

#define SOLO_LINE_OFF      SOLO_LINE_LOW
#define SOLO_LINE_ON       SOLO_LINE_HIGH
#define SOLO_LINE_INACTIVE SOLO_LINE_LOW
#define SOLO_LINE_ACTIVE   SOLO_LINE_HIGH

/*
 * =============================================================================
 * Canonical I/O and System Status Enum
 * =============================================================================
 * Unifies return codes across all solo subsystems (socket, serial, parallel,
 * sandbox, filesystem).
 */
typedef enum solo_status {
    SOLO_OK          =  0, /* Operation succeeded (full or partial data transferred) */
    SOLO_ERROR       = -1, /* Fatal I/O, descriptor, or operating system error */
    SOLO_AGAIN       = -2, /* Non-blocking call would block, or timeout expired */
    SOLO_EOF         = -3, /* End of stream / peer connection closed gracefully */
    SOLO_UNSUPPORTED = -4  /* Operation or hardware mode not supported on platform */
} solo_status_t;

/* Backward-compatible subsystem aliases */
typedef solo_status_t solo_socket_status_t;
typedef solo_status_t solo_serial_status_t;
typedef solo_status_t solo_parallel_status_t;
typedef solo_status_t solo_sandbox_status_t;
typedef solo_status_t solo_fs_status_t;

#define SOLO_SOCK_OK            SOLO_OK
#define SOLO_SOCK_ERROR         SOLO_ERROR
#define SOLO_SOCK_AGAIN         SOLO_AGAIN
#define SOLO_SOCK_CLOSED        SOLO_EOF

#define SOLO_SERIAL_OK          SOLO_OK
#define SOLO_SERIAL_ERROR       SOLO_ERROR
#define SOLO_SERIAL_UNSUPPORTED SOLO_UNSUPPORTED

#define SOLO_PARALLEL_OK          SOLO_OK
#define SOLO_PARALLEL_ERROR       SOLO_ERROR
#define SOLO_PARALLEL_UNSUPPORTED SOLO_UNSUPPORTED

#define SOLO_SANDBOX_OK          SOLO_OK
#define SOLO_SANDBOX_ERROR       SOLO_ERROR
#define SOLO_SANDBOX_UNSUPPORTED SOLO_UNSUPPORTED

#define SOLO_FS_OK          SOLO_OK
#define SOLO_FS_ERROR       SOLO_ERROR
#define SOLO_FS_UNSUPPORTED SOLO_UNSUPPORTED
#define SOLO_FS_EOF         SOLO_EOF

/*
 * Returns a static descriptive error string corresponding to a solo_status_t code.
 * Returned strings reside in static read-only storage and must never be freed.
 */
const char *solo_strerror(solo_status_t status);

/*
 * Safe C89 error buffer formatting helpers.
 * Formats message into err_buf (if err_buf != NULL && err_cap > 0).
 * Guarantees NUL termination within err_cap without heap allocation.
 */
void solo_set_err(char *err_buf, size_t err_cap, const char *msg);
void solo_set_err_fmt(char *err_buf, size_t err_cap, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_TYPES_H */
