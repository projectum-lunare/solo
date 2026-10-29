/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SOLO_SERIAL_H
#define SOLO_SERIAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct serial_port solo_serial_t;

typedef struct {
    int baud;         /* e.g. 9600, 19200, 38400, 115200 */
    int data_bits;    /* 5, 6, 7, 8 */
    char parity;      /* 'n' (none), 'e' (even), 'o' (odd) */
    int stop_bits;    /* 1, 2 */
    int flow_control; /* 0: none, 1: hardware (RTS/CTS), 2: software (XON/XOFF) */
    int rts;          /* -1: ignore, 0: off/clear, 1: on/set */
    int dtr;          /* -1: ignore, 0: off/clear, 1: on/set */
} solo_serial_opts_t;

/*
 * Opens a serial port for communication with impact printers.
 * Supports hardware flow control (RTS/CTS) and software flow control (XON/XOFF),
 * as well as initial modem line configuration for RTS and DTR.
 * On failure, returns NULL and populates *err_msg with a dynamically
 * allocated string (if err_msg != NULL). The caller owns the string and
 * must release it with free().
 */
solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char **err_msg);

/*
 * Writes data to the serial port. Returns the number of bytes written,
 * or -1 on error.
 */
int solo_serial_write(solo_serial_t *port, const char *buf, size_t len);

/*
 * Waits for the UART transmit buffer to drain completely (hardware drain).
 */
int solo_serial_drain(solo_serial_t *port);

/*
 * Controls the RTS and DTR modem lines of the UART.
 * Returns 0 on success, or -1 on error.
 */
int solo_serial_set_rts(solo_serial_t *port, int on);
int solo_serial_set_dtr(solo_serial_t *port, int on);

/*
 * Closes the serial port and frees allocated resources.
 */
void solo_serial_close(solo_serial_t *port);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_SERIAL_H */
