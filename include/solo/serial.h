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

/*
 * Serial operation status codes.
 */
typedef enum {
    SOLO_SERIAL_OK          =  0,
    SOLO_SERIAL_ERROR       = -1,
    SOLO_SERIAL_UNSUPPORTED = -2
} solo_serial_status_t;

/*
 * Serial flow control modes.
 */
typedef enum {
    SOLO_SERIAL_FLOW_NONE     = 0,
    SOLO_SERIAL_FLOW_HARDWARE = 1, /* RTS/CTS */
    SOLO_SERIAL_FLOW_SOFTWARE = 2  /* XON/XOFF */
} solo_serial_flow_t;

/*
 * Serial parity modes.
 */
typedef enum {
    SOLO_SERIAL_PARITY_NONE  = 'n',
    SOLO_SERIAL_PARITY_EVEN  = 'e',
    SOLO_SERIAL_PARITY_ODD   = 'o',
    SOLO_SERIAL_PARITY_MARK  = 'm',
    SOLO_SERIAL_PARITY_SPACE = 's'
} solo_serial_parity_t;

/*
 * Serial stop bit counts.
 */
typedef enum {
    SOLO_SERIAL_STOP_1 = 1,
    SOLO_SERIAL_STOP_2 = 2
} solo_serial_stop_bits_t;

/*
 * Serial data bit lengths.
 */
typedef enum {
    SOLO_SERIAL_DATA_5 = 5,
    SOLO_SERIAL_DATA_6 = 6,
    SOLO_SERIAL_DATA_7 = 7,
    SOLO_SERIAL_DATA_8 = 8
} solo_serial_data_bits_t;

/*
 * Standard serial baud rates.
 */
typedef enum {
    SOLO_SERIAL_BAUD_300    = 300,
    SOLO_SERIAL_BAUD_1200   = 1200,
    SOLO_SERIAL_BAUD_2400   = 2400,
    SOLO_SERIAL_BAUD_4800   = 4800,
    SOLO_SERIAL_BAUD_9600   = 9600,
    SOLO_SERIAL_BAUD_19200  = 19200,
    SOLO_SERIAL_BAUD_38400  = 38400,
    SOLO_SERIAL_BAUD_57600  = 57600,
    SOLO_SERIAL_BAUD_115200 = 115200
} solo_serial_baud_t;

/*
 * Serial modem line states (RTS/DTR).
 */
typedef enum {
    SOLO_SERIAL_LINE_IGNORE = -1,
    SOLO_SERIAL_LINE_OFF    =  0,
    SOLO_SERIAL_LINE_ON     =  1
} solo_serial_line_t;

typedef struct {
    int baud;         /* solo_serial_baud_t or integer (e.g. 9600, 115200) */
    int data_bits;    /* solo_serial_data_bits_t (5, 6, 7, 8) */
    char parity;      /* solo_serial_parity_t ('n', 'e', 'o', 'm', 's') */
    int stop_bits;    /* solo_serial_stop_bits_t (1, 2) */
    int flow_control; /* solo_serial_flow_t (0: none, 1: hardware RTS/CTS, 2: software XON/XOFF) */
    int rts;          /* solo_serial_line_t (-1: ignore, 0: off/clear, 1: on/set) */
    int dtr;          /* solo_serial_line_t (-1: ignore, 0: off/clear, 1: on/set) */
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
 * Returns SOLO_SERIAL_OK on success, SOLO_SERIAL_ERROR on I/O error,
 * or SOLO_SERIAL_UNSUPPORTED if modem line control is not supported.
 */
solo_serial_status_t solo_serial_set_rts(solo_serial_t *port, solo_serial_line_t state);
solo_serial_status_t solo_serial_set_dtr(solo_serial_t *port, solo_serial_line_t state);

/*
 * Closes the serial port and frees allocated resources.
 */
void solo_serial_close(solo_serial_t *port);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_SERIAL_H */
