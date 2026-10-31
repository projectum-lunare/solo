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
#include <solo/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct parallel_port solo_parallel_t;

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
 * On failure, returns NULL and writes descriptive error text into err_buf (if non-NULL).
 * Does not allocate dynamic heap memory for error strings.
 */
solo_parallel_t *solo_parallel_open(const char *path, char *err_buf, size_t err_cap);

/*
 * Writes data to the parallel port. Returns SOLO_OK on success (with *written_bytes populated),
 * or SOLO_ERROR on failure.
 */
solo_status_t solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len, size_t *written_bytes);

/*
 * Reads reverse data from the parallel port (IEEE 1284 Nibble/Byte/EPP/ECP modes).
 * Returns SOLO_OK on success (with *read_bytes populated), SOLO_UNSUPPORTED if mode/hardware
 * does not support reverse reading, or SOLO_ERROR on failure.
 */
solo_status_t solo_parallel_read(solo_parallel_t *port, char *buf, size_t max_len, size_t *read_bytes);

/*
 * Queries current Centronics status pins (BUSY, ACK, PE, SLCT, FAULT).
 * Returns SOLO_PARALLEL_OK and populates *pins_out with solo_parallel_pin_t bitmask,
 * or SOLO_PARALLEL_UNSUPPORTED / SOLO_PARALLEL_ERROR on failure.
 */
solo_parallel_status_t solo_parallel_get_status(solo_parallel_t *port, int *pins_out);

/*
 * Configures the operational mode (SPP, NIBBLE, BYTE, EPP, ECP).
 * Returns SOLO_PARALLEL_OK, or SOLO_PARALLEL_UNSUPPORTED if the mode
 * is not supported by the hardware or driver.
 */
solo_parallel_status_t solo_parallel_set_mode(solo_parallel_t *port, solo_parallel_mode_t mode);

/*
 * Waits for the printer to empty its hardware buffer and deassert BUSY (hardware drain).
 * Returns SOLO_PARALLEL_OK on success, or SOLO_PARALLEL_ERROR on timeout or failure.
 */
solo_parallel_status_t solo_parallel_drain(solo_parallel_t *port);

/*
 * Closes the parallel port and frees allocated resources.
 */
void solo_parallel_close(solo_parallel_t *port);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_PARALLEL_H */
