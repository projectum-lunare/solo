/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <solo/platform.h>

#if SOLO_IS_ATARIST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <osbind.h>

#include <solo/serial.h>

struct serial_port {
    short dev_id; /* BIOS device 1 = AUX: (RS-232 / Modem) */
    int timeout_ms;
    char name[16];
};

/*
 * Maps standard baud rate in bps to Atari ST XBIOS Rsconf baud code:
 * 0: 19200, 1: 9600, 2: 4800, 3: 3600, 4: 2400, 5: 2000,
 * 6: 1800, 7: 1200, 8: 600, 9: 300 bps.
 */
static short map_atari_baud(int baud)
{
    switch (baud) {
    case 19200:
        return 0;
    case 9600:
        return 1;
    case 4800:
        return 2;
    case 3600:
        return 3;
    case 2400:
        return 4;
    case 2000:
        return 5;
    case 1800:
        return 6;
    case 1200:
        return 7;
    case 600:
        return 8;
    case 300:
        return 9;
    default:
        if (baud > 9600)
            return 0; /* 19200 fallback */
        return 1;     /* 9600 default */
    }
}

/*
 * Computes Atari ST MFP 68901 USART Control Register (UCR) byte:
 * - Bit 7: /16 clock divide factor (0x80)
 * - Bits 6-5: Word length (00: 8 bits, 01: 7 bits, 10: 6 bits, 11: 5 bits)
 * - Bits 4-3: Stop bits (01: 1 stop bit, 11: 2 stop bits)
 * - Bits 2-1: Parity (0x00: none, 0x06: even, 0x04: odd)
 */
static short compute_atari_ucr(int data_bits, char parity, int stop_bits)
{
    short ucr = 0x80; /* Divide by 16 clock */

    /* Data bits (bits 6-5) */
    if (data_bits == 7) {
        ucr |= 0x20;
    } else if (data_bits == 6) {
        ucr |= 0x40;
    } else if (data_bits == 5) {
        ucr |= 0x60;
    } else {
        ucr |= 0x00; /* 8 data bits */
    }

    /* Stop bits (bits 4-3) */
    if (stop_bits == 2) {
        ucr |= 0x18; /* 2 stop bits */
    } else {
        ucr |= 0x08; /* 1 stop bit */
    }

    /* Parity (bits 2-1) */
    if (parity == 'e' || parity == 'E') {
        ucr |= 0x06; /* Parity enabled, even */
    } else if (parity == 'o' || parity == 'O') {
        ucr |= 0x04; /* Parity enabled, odd */
    }

    return ucr;
}

solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char *err_buf, size_t err_cap)
{
    solo_serial_t *port;
    (void)path;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    port = (solo_serial_t *)malloc(sizeof(solo_serial_t));
    if (port == NULL) {
        solo_set_err(err_buf, err_cap, "Out of memory allocating serial port handle");
        return NULL;
    }

    port->dev_id = 1; /* BIOS Device 1: RS-232 Serial (AUX:) */
    port->timeout_ms = (opts != NULL) ? opts->timeout_ms : -1;
    strncpy(port->name, (path && path[0] != '\0') ? path : "AUX:", sizeof(port->name) - 1);
    port->name[sizeof(port->name) - 1] = '\0';

    if (opts != NULL) {
        short baud_code = (opts->baud > 0) ? map_atari_baud(opts->baud) : -1;
        short flow_code = 0; /* None */
        short ucr_code;

        if (opts->flow_control == 1) {
            flow_code = 2; /* Hardware RTS/CTS */
        } else if (opts->flow_control == 2) {
            flow_code = 1; /* Software XON/XOFF */
        }

        ucr_code = compute_atari_ucr(opts->data_bits, opts->parity, opts->stop_bits);

        /* Configure MFP 68901 UART via XBIOS Rsconf */
        Rsconf(baud_code, flow_code, ucr_code, -1, -1, -1);

        /* Initial modem line states */
        if (opts->rts != -1) {
            solo_serial_set_rts(port, (solo_serial_line_t)opts->rts);
        }
        if (opts->dtr != -1) {
            solo_serial_set_dtr(port, (solo_serial_line_t)opts->dtr);
        }
    }

    return port;
}

solo_status_t solo_serial_write(solo_serial_t *port, const char *buf, size_t len, size_t *written_bytes)
{
    size_t i;
    clock_t start = 0;
    clock_t wait_ticks = 0;
    int has_timeout = 0;

    if (written_bytes != NULL) {
        *written_bytes = 0;
    }

    if (port == NULL || buf == NULL) {
        return SOLO_ERROR;
    }
    if (len == 0) {
        return SOLO_OK;
    }

    if (port->timeout_ms > 0) {
        has_timeout = 1;
        start = clock();
        wait_ticks = (clock_t)(((long)port->timeout_ms * CLOCKS_PER_SEC) / 1000L);
        if (wait_ticks == 0) wait_ticks = 1;
    }

    for (i = 0; i < len; i++) {
        if (has_timeout) {
            while (Bcostat(port->dev_id) == 0) {
                if ((clock() - start) >= wait_ticks) {
                    if (written_bytes != NULL) *written_bytes = i;
                    return (i > 0) ? SOLO_OK : SOLO_AGAIN;
                }
            }
        } else if (port->timeout_ms == 0) {
            if (Bcostat(port->dev_id) == 0) {
                if (written_bytes != NULL) *written_bytes = i;
                return (i > 0) ? SOLO_OK : SOLO_AGAIN;
            }
        } else {
            while (Bcostat(port->dev_id) == 0) {
                /* wait indefinitely */
            }
        }

        Bconout(port->dev_id, (unsigned char)buf[i]);
    }

    if (written_bytes != NULL) {
        *written_bytes = len;
    }
    return SOLO_OK;
}

solo_status_t solo_serial_read(solo_serial_t *port, char *buf, size_t max_len, size_t *read_bytes, int timeout_ms)
{
    int eff_timeout;

    if (read_bytes != NULL) {
        *read_bytes = 0;
    }

    if (port == NULL || buf == NULL || max_len == 0) {
        return SOLO_ERROR;
    }

    eff_timeout = (timeout_ms >= 0) ? timeout_ms : port->timeout_ms;

    if (eff_timeout > 0) {
        clock_t start = clock();
        clock_t wait_ticks = (clock_t)(((long)eff_timeout * CLOCKS_PER_SEC) / 1000L);
        if (wait_ticks == 0) wait_ticks = 1;
        while (Bconstat(port->dev_id) == 0) {
            if ((clock() - start) >= wait_ticks) {
                return SOLO_AGAIN; /* Timeout */
            }
        }
    } else if (eff_timeout == 0) {
        if (Bconstat(port->dev_id) == 0) {
            return SOLO_AGAIN; /* Would block */
        }
    } else {
        while (Bconstat(port->dev_id) == 0) {
            /* wait indefinitely */
        }
    }

    buf[0] = (char)Bconin(port->dev_id);
    if (read_bytes != NULL) {
        *read_bytes = 1;
    }
    return SOLO_OK;
}

solo_serial_status_t solo_serial_set_timeout(solo_serial_t *port, int timeout_ms)
{
    if (port == NULL) {
        return SOLO_SERIAL_ERROR;
    }
    port->timeout_ms = timeout_ms;
    return SOLO_SERIAL_OK;
}

int solo_serial_get_cts(solo_serial_t *port)
{
    (void)port;
    return -1;
}

int solo_serial_get_dsr(solo_serial_t *port)
{
    (void)port;
    return -1;
}

int solo_serial_get_cd(solo_serial_t *port)
{
    (void)port;
    return -1;
}

int solo_serial_get_ri(solo_serial_t *port)
{
    (void)port;
    return -1;
}

int solo_serial_drain(solo_serial_t *port)
{
    long timeout = 2000000L;

    if (port == NULL) {
        return -1;
    }

    /* Wait until transmitter buffer has emptied */
    while (Bcostat(port->dev_id) == 0) {
        if (--timeout <= 0) {
            return -1;
        }
    }

    return 0;
}

solo_serial_status_t solo_serial_set_rts(solo_serial_t *port, solo_serial_line_t state)
{
    if (port == NULL) {
        return SOLO_SERIAL_ERROR;
    }

    /* Toggle RTS control via XBIOS Rsconf */
    Rsconf(-1, (state == SOLO_SERIAL_LINE_ON || (int)state == 1) ? 2 : 0, -1, -1, -1, -1);
    return SOLO_SERIAL_OK;
}

solo_serial_status_t solo_serial_set_dtr(solo_serial_t *port, solo_serial_line_t state)
{
    if (port == NULL) {
        return SOLO_SERIAL_ERROR;
    }

    /*
     * On Atari ST hardware, DTR is connected to Port A Bit 4 of the
     * Yamaha YM2149 sound chip with inverted logic:
     * - Offgibit clears bit 4 (asserting DTR high)
     * - Ongibit sets bit 4 (deasserting DTR low)
     */
    if (state == SOLO_SERIAL_LINE_ON || (int)state == 1) {
        Offgibit((short)~0x10);
    } else {
        Ongibit(0x10);
    }
    return SOLO_SERIAL_OK;
}

void solo_serial_close(solo_serial_t *port)
{
    if (port != NULL) {
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_serial_atarist;
#endif /* SOLO_IS_ATARIST */
