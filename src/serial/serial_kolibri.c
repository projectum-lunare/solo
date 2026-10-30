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

#if SOLO_IS_KOLIBRI

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/serial.h>

struct serial_port {
    uint16_t base;    /* UART base I/O port address (e.g. 0x3F8) */
    int flow_control; /* 0: none, 1: hardware (RTS/CTS), 2: software (XON/XOFF) */
    int timeout_ms;   /* Read/write timeout in milliseconds */
    uint8_t orig_ier; /* Original Interrupt Enable Register */
    uint8_t orig_lcr; /* Original Line Control Register */
    uint8_t orig_mcr; /* Original Modem Control Register */
};

/*
 * Low-level x86 I/O port primitives (GCC inline assembly)
 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t data;
    __asm__ __volatile__("inb %w1, %b0" : "=a"(data) : "Nd"(port));
    return data;
}

static inline void outb(uint16_t port, uint8_t data)
{
    __asm__ __volatile__("outb %b0, %w1" : : "a"(data), "Nd"(port));
}

/*
 * KolibriOS System Call Wrappers (int 0x40):
 *
 * Function 5: Sleep / Delay
 *   eax = 5
 *   ebx = duration in 1/100ths of a second (centiseconds)
 *   ebx = 0 yields the remainder of the current timeslice.
 */
static inline void kolibri_sleep(uint32_t centiseconds)
{
    int dummy;
    __asm__ __volatile__("int $0x40" : "=a"(dummy) : "a"(5), "b"(centiseconds));
    (void)dummy;
}

/*
 * Function 46: Request / Free I/O Port access from KolibriOS kernel
 *   eax = 46
 *   ebx = 0 (reserve), 1 (free)
 *   ecx = base port address
 *   edx = number of ports
 */
static void kolibri_reserve_io_port(uint16_t port, uint16_t count)
{
    int dummy;
    __asm__ __volatile__("int $0x40"
                         : "=a"(dummy)
                         : "a"(46), "b"(0), "c"((uint32_t)port), "d"((uint32_t)count));
    (void)dummy;
}

static void kolibri_free_io_port(uint16_t port, uint16_t count)
{
    int dummy;
    __asm__ __volatile__("int $0x40"
                         : "=a"(dummy)
                         : "a"(46), "b"(1), "c"((uint32_t)port), "d"((uint32_t)count));
    (void)dummy;
}

static uint16_t parse_kolibri_port(const char *path)
{
    static const uint16_t uarts[4] = {0x3F8, 0x2F8, 0x3E8, 0x2E8};

    if (path == NULL || path[0] == '\0') {
        return 0x3F8;
    }
    if (path[0] == '0' && (path[1] == 'x' || path[1] == 'X')) {
        return (uint16_t)strtoul(path, NULL, 16);
    }
    if (path[0] >= '1' && path[0] <= '4' && path[1] == '\0') {
        return uarts[path[0] - '1'];
    }
    return 0;
}

/*
 * Hardware UART presence detection.
 * Tests the 16550 Scratch Register (offset 7), and falls back to MCR loopback.
 */
static int test_uart_present(uint16_t base)
{
    uint8_t old = inb(base + 7);
    outb(base + 7, 0x55);
    if (inb(base + 7) == 0x55) {
        outb(base + 7, 0xAA);
        if (inb(base + 7) == 0xAA) {
            outb(base + 7, old);
            return 1;
        }
    }

    /* Fallback: test MCR loopback register */
    {
        uint8_t orig_mcr = inb(base + 4);
        outb(base + 4, 0x1E); /* Loopback, RTS=1, DTR=0, OUT1=1, OUT2=1 */
        uint8_t msr = inb(base + 6);
        outb(base + 4, orig_mcr);
        if ((msr & 0xB0) == 0xB0) {
            return 1;
        }
    }

    return 0;
}

solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char **err_msg)
{
    uint16_t base;
    solo_serial_t *port;
    solo_serial_opts_t default_opts;
    uint16_t divisor;
    uint8_t lcr = 0;
    uint8_t mcr = 0;
    int baud;

    if (err_msg != NULL) {
        *err_msg = NULL;
    }

    if (opts == NULL) {
        default_opts.baud = 9600;
        default_opts.data_bits = 8;
        default_opts.parity = 'n';
        default_opts.stop_bits = 1;
        default_opts.flow_control = 0; /* safe default: none */
        default_opts.rts = 1;
        default_opts.dtr = 1;
        default_opts.timeout_ms = 0;
        opts = &default_opts;
    }

    if (path == NULL) {
        if (err_msg)
            *err_msg = safe_strdup("Device path cannot be NULL");
        return NULL;
    }

    base = parse_kolibri_port(path);
    if (base == 0) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "Unknown KolibriOS serial port '%s' (use COM1..COM4 or 0x3F8)", path);
            *err_msg = safe_strdup(buf);
        }
        return NULL;
    }

    /* Request 8 contiguous I/O ports from KolibriOS kernel */
    kolibri_reserve_io_port(base, 8);

    /* Verify that UART hardware exists at this address */
    if (!test_uart_present(base)) {
        kolibri_free_io_port(base, 8);
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "No UART hardware detected at I/O port 0x%03X (%s)", base,
                     path);
            *err_msg = safe_strdup(buf);
        }
        return NULL;
    }

    port = (solo_serial_t *)malloc(sizeof(solo_serial_t));
    if (port == NULL) {
        kolibri_free_io_port(base, 8);
        if (err_msg)
            *err_msg = safe_strdup("Memory allocation failure");
        return NULL;
    }

    port->base = base;
    port->flow_control = opts->flow_control;
    port->timeout_ms = opts->timeout_ms;

    /* Save original register configurations */
    port->orig_ier = inb(base + 1);
    port->orig_lcr = inb(base + 3);
    port->orig_mcr = inb(base + 4);

    /* 1. Disable all UART interrupts (polling mode) */
    outb(base + 1, 0x00);

    /* 2. Configure baud rate divisor */
    baud = (opts->baud > 0) ? opts->baud : 9600;
    divisor = (uint16_t)(115200 / baud);
    if (divisor == 0)
        divisor = 1;

    outb(base + 3, 0x80); /* Set DLAB (Divisor Latch Access Bit) */
    outb(base + 0, (uint8_t)(divisor & 0xFF));
    outb(base + 1, (uint8_t)((divisor >> 8) & 0xFF));

    /* 3. Configure Line Control Register (LCR) */
    switch (opts->data_bits) {
    case 5:
        lcr |= 0x00;
        break;
    case 6:
        lcr |= 0x01;
        break;
    case 7:
        lcr |= 0x02;
        break;
    case 8:
    default:
        lcr |= 0x03;
        break;
    }

    if (opts->stop_bits == 2) {
        lcr |= 0x04;
    }

    if (opts->parity == 'e' || opts->parity == 'E') {
        lcr |= 0x18; /* Even parity */
    } else if (opts->parity == 'o' || opts->parity == 'O') {
        lcr |= 0x08; /* Odd parity */
    }

    /* Writing LCR automatically clears DLAB */
    outb(base + 3, lcr);

    /* 4. Enable FIFO, clear TX and RX FIFOs, 14-byte threshold */
    outb(base + 2, 0xC7);

    /* 5. Configure Modem Control Register (MCR) */
    mcr = 0x08; /* OUT2 (standard for PC UARTs) */
    if (opts->rts != 0)
        mcr |= 0x02; /* RTS enable */
    if (opts->dtr != 0)
        mcr |= 0x01; /* DTR enable */
    outb(base + 4, mcr);

    return port;
}

solo_serial_status_t solo_serial_set_rts(solo_serial_t *port, solo_serial_line_t state)
{
    uint8_t mcr;
    if (port == NULL || port->base == 0)
        return SOLO_SERIAL_ERROR;
    mcr = inb(port->base + 4);
    if (state == SOLO_SERIAL_LINE_ON || (int)state == 1)
        mcr |= 0x02;
    else
        mcr &= ~0x02;
    outb(port->base + 4, mcr);
    return SOLO_SERIAL_OK;
}

solo_serial_status_t solo_serial_set_dtr(solo_serial_t *port, solo_serial_line_t state)
{
    uint8_t mcr;
    if (port == NULL || port->base == 0)
        return SOLO_SERIAL_ERROR;
    mcr = inb(port->base + 4);
    if (state == SOLO_SERIAL_LINE_ON || (int)state == 1)
        mcr |= 0x01;
    else
        mcr &= ~0x01;
    outb(port->base + 4, mcr);
    return SOLO_SERIAL_OK;
}

int solo_serial_write(solo_serial_t *port, const char *buf, size_t len)
{
    size_t i;
    uint16_t base;
    int elapsed = 0;

    if (port == NULL || port->base == 0 || buf == NULL)
        return -1;
    if (len == 0)
        return 0;

    base = port->base;

    for (i = 0; i < len; i++) {
        /*
         * Hardware Flow Control (RTS/CTS):
         * Wait until CTS (Clear To Send, MSR bit 4 - 0x10) is asserted.
         */
        if (port->flow_control == 1) {
            while ((inb(base + 6) & 0x10) == 0) {
                if (port->timeout_ms > 0) {
                    kolibri_sleep(1); /* 10ms */
                    elapsed += 10;
                    if (elapsed >= port->timeout_ms) {
                        return (int)i;
                    }
                } else if (port->timeout_ms == 0) {
                    return (int)i;
                } else {
                    kolibri_sleep(0);
                }
            }
        }

        /* Wait for Transmitter Holding Register Empty (LSR bit 5 - 0x20) */
        while ((inb(base + 5) & 0x20) == 0) {
            if (port->timeout_ms > 0) {
                kolibri_sleep(1); /* 10ms */
                elapsed += 10;
                if (elapsed >= port->timeout_ms) {
                    return (int)i;
                }
            } else if (port->timeout_ms == 0) {
                return (int)i;
            } else {
                kolibri_sleep(0); /* Yield */
            }
        }

        /* Transmit byte */
        outb(base + 0, (uint8_t)buf[i]);
    }

    return (int)len;
}

int solo_serial_read(solo_serial_t *port, char *buf, size_t max_len, int timeout_ms)
{
    int eff_timeout;
    if (port == NULL || port->base == 0 || buf == NULL || max_len == 0)
        return -1;

    eff_timeout = (timeout_ms >= 0) ? timeout_ms : port->timeout_ms;

    if (eff_timeout > 0) {
        int elapsed = 0;
        while ((inb(port->base + 5) & 0x01) == 0) {
            kolibri_sleep(1); /* 10ms */
            elapsed += 10;
            if (elapsed >= eff_timeout) {
                return 0; /* Timeout */
            }
        }
    } else if (eff_timeout == 0) {
        if ((inb(port->base + 5) & 0x01) == 0) {
            return 0; /* Would block */
        }
    } else {
        while ((inb(port->base + 5) & 0x01) == 0) {
            kolibri_sleep(0); /* Yield */
        }
    }

    buf[0] = (char)inb(port->base + 0);
    return 1;
}

solo_serial_status_t solo_serial_set_timeout(solo_serial_t *port, int timeout_ms)
{
    if (port == NULL || port->base == 0)
        return SOLO_SERIAL_ERROR;
    port->timeout_ms = timeout_ms;
    return SOLO_SERIAL_OK;
}

int solo_serial_get_cts(solo_serial_t *port)
{
    if (port == NULL || port->base == 0) return -1;
    return (inb(port->base + 6) & 0x10) ? 1 : 0;
}

int solo_serial_get_dsr(solo_serial_t *port)
{
    if (port == NULL || port->base == 0) return -1;
    return (inb(port->base + 6) & 0x20) ? 1 : 0;
}

int solo_serial_get_ri(solo_serial_t *port)
{
    if (port == NULL || port->base == 0) return -1;
    return (inb(port->base + 6) & 0x40) ? 1 : 0;
}

int solo_serial_get_cd(solo_serial_t *port)
{
    if (port == NULL || port->base == 0) return -1;
    return (inb(port->base + 6) & 0x80) ? 1 : 0;
}

int solo_serial_drain(solo_serial_t *port)
{
    uint16_t base;
    uint32_t timeout_loops = 0;

    if (port == NULL || port->base == 0)
        return -1;
    base = port->base;

    /*
     * Line Status Register (LSR) bit 6 (TEMT):
     * Transmitter Empty indicates both Transmitter Holding Register and
     * Transmitter Shift Register are completely empty.
     */
    while ((inb(base + 5) & 0x40) == 0) {
        kolibri_sleep(1);             /* Wait 10ms */
        if (++timeout_loops > 1000) { /* 10-second drain timeout */
            return -1;
        }
    }
    return 0;
}

void solo_serial_close(solo_serial_t *port)
{
    if (port != NULL) {
        if (port->base != 0) {
            /* Drain any remaining bytes */
            solo_serial_drain(port);

            /* Restore original register states */
            outb(port->base + 1, port->orig_ier);
            outb(port->base + 3, port->orig_lcr);
            outb(port->base + 4, port->orig_mcr);

            /* Release kernel I/O port reservation */
            kolibri_free_io_port(port->base, 8);
            port->base = 0;
        }
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_serial_kolibri;
#endif /* SOLO_IS_KOLIBRI */
