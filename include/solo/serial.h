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
#include <solo/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct serial_port solo_serial_t;

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
    int timeout_ms;   /* Read/write timeout: 0 = non-blocking, >0 = timeout in ms, <0 = blocking indefinitely */
} solo_serial_opts_t;

/*
 * Opens a serial port for communication.
 * Default flow control is SOLO_SERIAL_FLOW_NONE (0).
 * Default timeout is -1 (blocking indefinitely).
 * Hardware flow control (RTS/CTS) is only activated when explicitly requested.
 * On failure, returns NULL and writes descriptive error text into err_buf (if non-NULL).
 * Does not allocate dynamic heap memory for error strings.
 */
solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char *err_buf, size_t err_cap);

/*
 * Writes data to the serial port.
 * Returns:
 *   SOLO_OK          : all requested bytes written, or partial bytes written before timeout.
 *   SOLO_AGAIN       : timeout expired before any bytes could be accepted.
 *   SOLO_ERROR       : fatal I/O or descriptor error.
 * If written_bytes != NULL, stores the count of bytes successfully accepted.
 */
solo_status_t solo_serial_write(solo_serial_t *port, const char *buf, size_t len, size_t *written_bytes);

/*
 * Reads data from the serial port.
 * Returns:
 *   SOLO_OK          : one or more bytes read into buf (*read_bytes populated).
 *   SOLO_AGAIN       : timeout expired before any bytes became available.
 *   SOLO_ERROR       : fatal I/O error or descriptor invalid.
 *   SOLO_UNSUPPORTED : timed read not supported on platform.
 *
 * If timeout_ms < 0, uses the port's configured timeout (from opts or solo_serial_set_timeout).
 * If the effective timeout is < 0, blocks indefinitely until at least one byte is available.
 * If the effective timeout is 0, performs a non-blocking check.
 * If the effective timeout is > 0, waits up to timeout_ms milliseconds.
 */
solo_status_t solo_serial_read(solo_serial_t *port, char *buf, size_t max_len, size_t *read_bytes, int timeout_ms);

/*
 * Configures the default I/O timeout in milliseconds for the serial port.
 */
solo_serial_status_t solo_serial_set_timeout(solo_serial_t *port, int timeout_ms);

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
 * Queries the modem input lines (CTS, DSR, CD/DCD, RI).
 * Returns 1 if asserted/high, 0 if deasserted/low,
 * or -1 on error or if unsupported.
 */
int solo_serial_get_cts(solo_serial_t *port);
int solo_serial_get_dsr(solo_serial_t *port);
int solo_serial_get_cd(solo_serial_t *port);
int solo_serial_get_ri(solo_serial_t *port);

/*
 * Closes the serial port and frees allocated resources.
 */
void solo_serial_close(solo_serial_t *port);

#ifdef __cplusplus
}
#endif

#endif /* SOLO_SERIAL_H */
