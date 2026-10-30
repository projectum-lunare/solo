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

#include <solo/parallel.h>

struct parallel_port {
    uint16_t base;     /* Base I/O address (e.g. 0x378 for LPT1) */
    uint8_t orig_ctrl; /* Original Control Register value */
};

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

static inline void kolibri_sleep(uint32_t centiseconds)
{
    int dummy;
    __asm__ __volatile__("int $0x40" : "=a"(dummy) : "a"(5), "b"(centiseconds));
    (void)dummy;
}

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

/*
 * Resolves logical port specification to standard PC parallel I/O port address.
 * Pre-resolved by Lua: canonical index "1".."3" (LPT1..LPT3) or direct hex (e.g. "0x378").
 */
static uint16_t parse_kolibri_lpt(const char *path)
{
    static const uint16_t lpts[3] = {0x378, 0x278, 0x3BC};

    if (path == NULL || path[0] == '\0') {
        return 0x378; /* Default LPT1 */
    }
    if (path[0] == '0' && (path[1] == 'x' || path[1] == 'X')) {
        return (uint16_t)strtoul(path, NULL, 16);
    }
    if (path[0] >= '1' && path[0] <= '3' && path[1] == '\0') {
        return lpts[path[0] - '1'];
    }
    return 0x378;
}

/*
 * Detects presence of standard PC parallel port data register latch.
 */
static int test_lpt_present(uint16_t base)
{
    uint8_t old = inb(base + 0);
    outb(base + 0, 0x55);
    if (inb(base + 0) != 0x55)
        return 0;
    outb(base + 0, 0xAA);
    if (inb(base + 0) != 0xAA)
        return 0;
    outb(base + 0, old);
    return 1;
}

solo_parallel_t *solo_parallel_open(const char *path, char *err_buf, size_t err_cap)
{
    uint16_t base;
    solo_parallel_t *port;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    base = parse_kolibri_lpt(path);
    if (base == 0) {
        solo_set_err_fmt(err_buf, err_cap,
                         "Unknown KolibriOS parallel port '%s' (use LPT1..LPT3 or 0x378)",
                         path ? path : "NULL");
        return NULL;
    }

    /* Reserve 3 I/O ports: Data (base+0), Status (base+1), Control (base+2) */
    kolibri_reserve_io_port(base, 3);

    if (!test_lpt_present(base)) {
        kolibri_free_io_port(base, 3);
        solo_set_err_fmt(err_buf, err_cap, "No parallel port hardware detected at I/O port 0x%03X",
                         base);
        return NULL;
    }

    port = (solo_parallel_t *)malloc(sizeof(solo_parallel_t));
    if (port == NULL) {
        kolibri_free_io_port(base, 3);
        solo_set_err(err_buf, err_cap, "Memory allocation failure");
        return NULL;
    }

    port->base = base;
    port->orig_ctrl = inb(base + 2);

    /* Initialize control register: SELECT IN active (bit 3), INIT inactive (bit 2) */
    outb(base + 2, 0x0C);

    return port;
}

solo_status_t solo_parallel_write(solo_parallel_t *port, const char *buf, size_t len, size_t *written_bytes)
{
    size_t i;
    uint16_t base;

    if (written_bytes != NULL) {
        *written_bytes = 0;
    }

    if (port == NULL || port->base == 0 || buf == NULL) {
        return SOLO_ERROR;
    }
    if (len == 0) {
        return SOLO_OK;
    }

    base = port->base;

    for (i = 0; i < len; i++) {
        uint32_t timeout_loops = 0;

        /*
         * Centronics Handshake:
         * Wait until printer deasserts BUSY line.
         * In Status Register (base + 1), Bit 7 is ~BUSY (1 = ready, 0 = busy).
         */
        while ((inb(base + 1) & 0x80) == 0) {
            /* Check for Paper Out (bit 5) or Error/Fault (bit 3 is ~FAULT) */
            uint8_t status = inb(base + 1);
            if (status & 0x20) {
                /* Paper End detected */
            }
            kolibri_sleep(1);             /* Wait 10ms */
            if (++timeout_loops > 3000) { /* 30-second timeout */
                if (written_bytes != NULL) *written_bytes = i;
                return (i > 0) ? SOLO_OK : SOLO_ERROR;
            }
        }

        /* 1. Place data byte on data bus (base + 0) */
        outb(base + 0, (uint8_t)buf[i]);

        /* 2. Pulse STROBE low (Control register bit 0 is inverted, so set bit 0) */
        outb(base + 2, 0x0D);
        inb(base + 1);        /* ~1µs bus read delay for minimum strobe pulse width */
        outb(base + 2, 0x0C); /* Deassert STROBE */
    }

    if (written_bytes != NULL) {
        *written_bytes = len;
    }
    return SOLO_OK;
}

solo_status_t solo_parallel_read(solo_parallel_t *port, char *buf, size_t max_len, size_t *read_bytes)
{
    (void)port;
    (void)buf;
    (void)max_len;
    if (read_bytes != NULL) *read_bytes = 0;
    return SOLO_UNSUPPORTED;
}

solo_parallel_status_t solo_parallel_get_status(solo_parallel_t *port, int *pins_out)
{
    uint8_t st;
    int pins = 0;
    if (port == NULL || port->base == 0 || pins_out == NULL) return SOLO_PARALLEL_ERROR;
    st = inb(port->base + 1);
    if (!(st & 0x80)) pins |= SOLO_PARALLEL_PIN_BUSY;
    if (!(st & 0x40)) pins |= SOLO_PARALLEL_PIN_ACK;
    if (st & 0x20)    pins |= SOLO_PARALLEL_PIN_PE;
    if (st & 0x10)    pins |= SOLO_PARALLEL_PIN_SLCT;
    if (!(st & 0x08)) pins |= SOLO_PARALLEL_PIN_FAULT;
    *pins_out = pins;
    return SOLO_PARALLEL_OK;
}

solo_parallel_status_t solo_parallel_set_mode(solo_parallel_t *port, solo_parallel_mode_t mode)
{
    if (port == NULL || port->base == 0) return SOLO_PARALLEL_ERROR;
    if (mode == SOLO_PARALLEL_MODE_SPP) return SOLO_PARALLEL_OK;
    return SOLO_PARALLEL_UNSUPPORTED;
}

int solo_parallel_drain(solo_parallel_t *port)
{
    uint16_t base;
    uint32_t timeout_loops = 0;

    if (port == NULL || port->base == 0)
        return -1;
    base = port->base;

    /* Wait until printer is not busy */
    while ((inb(base + 1) & 0x80) == 0) {
        kolibri_sleep(1);
        if (++timeout_loops > 1000) { /* 10-second drain timeout */
            return -1;
        }
    }
    return 0;
}

void solo_parallel_close(solo_parallel_t *port)
{
    if (port != NULL) {
        if (port->base != 0) {
            solo_parallel_drain(port);
            outb(port->base + 2, port->orig_ctrl);
            kolibri_free_io_port(port->base, 3);
            port->base = 0;
        }
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_parallel_kolibri;
#endif /* SOLO_IS_KOLIBRI */
