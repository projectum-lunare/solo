/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_PARALLEL_H
#define SOLO_PARALLEL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct parallel_port solo_parallel_t;

/*
 * Parallel operation status codes.
 */
typedef enum {
    SOLO_PARALLEL_OK          =  0,
    SOLO_PARALLEL_ERROR       = -1,
    SOLO_PARALLEL_UNSUPPORTED = -2
} solo_parallel_status_t;

/*
 * IEEE 1284 / Centronics parallel port modes.
 */
typedef enum {
    SOLO_PARALLEL_MODE_SPP    = 0, /* Standard Parallel Port (Centronics compatibility) */
    SOLO_PARALLEL_MODE_NIBBLE = 1, /* IEEE 1284 4-bit reverse read */
    SOLO_PARALLEL_MODE_BYTE   = 2, /* IEEE 1284 8-bit bidirectional (PS/2) */
    SOLO_PARALLEL_MODE_EPP    = 3, /* Enhanced Parallel Port */
    SOLO_PARALLEL_MODE_ECP    = 4  /* Extended Capabilities Port (FIFO hardware) */
} solo_parallel_mode_t;

/*
 * Centronics status pins & handshake line flags.
 */
typedef enum {
    SOLO_PARALLEL_PIN_BUSY  = (1 << 0), /* Printer busy */
    SOLO_PARALLEL_PIN_ACK   = (1 << 1), /* Acknowledge pulse */
    SOLO_PARALLEL_PIN_PE    = (1 << 2), /* Paper End / Out */
    SOLO_PARALLEL_PIN_SLCT  = (1 << 3), /* Select / Online */
    SOLO_PARALLEL_PIN_FAULT = (1 << 4)  /* Error / Fault */
} solo_parallel_pin_t;

/*
 * Opens a parallel / Centronics port (IEEE 1284 / LPT) for direct
 * communication with impact printers.
 * On failure, returns NULL and populates *err_msg with a dynamically
 * allocated string (if err_msg != NULL). The caller owns the string and
 * must release it with free().
 */
solo_parallel_t *solo_parallel_open(const char *path, char **err_msg);

/*
 * Writes data to the parallel port. Returns the number of bytes written,
 * or -1 on error.
 */
int solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len);

/*
 * Waits for the printer to empty its hardware buffer (hardware drain).
 */
int solo_parallel_drain(solo_parallel_t *port);

/*
 * Closes the parallel port and frees allocated resources.
 */
void solo_parallel_close(solo_parallel_t *port);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_PARALLEL_H */
