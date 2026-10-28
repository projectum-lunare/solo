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
#include <osbind.h>

#include <solo/serial.h>

struct serial_port {
    short dev_id; /* BIOS device 1 = AUX: (RS-232 / Modem) */
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

solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char **err_msg)
{
    solo_serial_t *port;
    (void)path;

    port = (solo_serial_t *)malloc(sizeof(solo_serial_t));
    if (port == NULL) {
        if (err_msg != NULL) {
            *err_msg = safe_strdup("Out of memory allocating serial port handle");
        }
        return NULL;
    }

    port->dev_id = 1; /* BIOS Device 1: RS-232 Serial (AUX:) */
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
            solo_serial_set_rts(port, opts->rts);
        }
        if (opts->dtr != -1) {
            solo_serial_set_dtr(port, opts->dtr);
        }
    }

    return port;
}

int solo_serial_write(solo_serial_t *port, const char *buf, size_t len)
{
    size_t i;

    if (port == NULL || buf == NULL) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        /* Poll Bcostat(1) until MFP 68901 UART transmitter is ready */
        long timeout = 2000000L;
        while (Bcostat(port->dev_id) == 0) {
            if (--timeout <= 0) {
                return (i > 0) ? (int)i : -1;
            }
        }

        Bconout(port->dev_id, (unsigned char)buf[i]);
    }

    return (int)len;
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

int solo_serial_set_rts(solo_serial_t *port, int on)
{
    if (port == NULL) {
        return -1;
    }

    /* Toggle RTS control via XBIOS Rsconf */
    Rsconf(-1, on ? 2 : 0, -1, -1, -1, -1);
    return 0;
}

int solo_serial_set_dtr(solo_serial_t *port, int on)
{
    if (port == NULL) {
        return -1;
    }

    /*
     * On Atari ST hardware, DTR is connected to Port A Bit 4 of the
     * Yamaha YM2149 sound chip with inverted logic:
     * - Offgibit clears bit 4 (asserting DTR high)
     * - Ongibit sets bit 4 (deasserting DTR low)
     */
    if (on) {
        Offgibit((short)~0x10);
    } else {
        Ongibit(0x10);
    }
    return 0;
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
