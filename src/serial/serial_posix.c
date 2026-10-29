#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

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

#if SOLO_IS_POSIX

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solo/serial.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

struct serial_port {
    int fd;
    struct termios orig_tio;
};

static speed_t get_posix_baud(int baud)
{
    switch (baud) {
    case 50:
        return B50;
    case 75:
        return B75;
    case 110:
        return B110;
    case 134:
        return B134;
    case 150:
        return B150;
    case 200:
        return B200;
    case 300:
        return B300;
    case 600:
        return B600;
    case 1200:
        return B1200;
    case 1800:
        return B1800;
    case 2400:
        return B2400;
    case 4800:
        return B4800;
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
#ifdef B57600
    case 57600:
        return B57600;
#endif
#ifdef B115200
    case 115200:
        return B115200;
#endif
    default:
        return B9600;
    }
}

solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char **err_msg)
{
    int fd;
    struct termios tio;
    solo_serial_t *port;
    speed_t speed;
    solo_serial_opts_t default_opts;

    const char *target = (path && path[0] != '\0') ? path : "/dev/ttyS0";

    if (opts == NULL) {
        default_opts.baud = 9600;
        default_opts.data_bits = 8;
        default_opts.parity = 'n';
        default_opts.stop_bits = 1;
        default_opts.flow_control = 1; /* hardware (RTS/CTS) */
        default_opts.rts = 1;
        default_opts.dtr = 1;
        opts = &default_opts;
    }

    /* Open device non-blocking first to avoid hanging on DCD */
    fd = open(target, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "open(%s) failed: %s", target, strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        return NULL;
    }

    port = (solo_serial_t *)malloc(sizeof(solo_serial_t));
    if (port == NULL) {
        close(fd);
        if (err_msg)
            *err_msg = safe_strdup("Memory allocation failure");
        return NULL;
    }
    port->fd = fd;

    if (tcgetattr(fd, &port->orig_tio) != 0) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "tcgetattr failed: %s", strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        close(fd);
        free(port);
        return NULL;
    }

    memset(&tio, 0, sizeof(tio));

    /* Control Flags: Enable receiver, local control line */
    tio.c_cflag |= (CREAD | CLOCAL);

    /* Data bits */
    tio.c_cflag &= ~CSIZE;
    if (opts->data_bits == 5)
        tio.c_cflag |= CS5;
    else if (opts->data_bits == 6)
        tio.c_cflag |= CS6;
    else if (opts->data_bits == 7)
        tio.c_cflag |= CS7;
    else
        tio.c_cflag |= CS8;

    /* Parity */
    if (opts->parity == 'e' || opts->parity == 'E') {
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
    } else if (opts->parity == 'o' || opts->parity == 'O') {
        tio.c_cflag |= PARENB;
        tio.c_cflag |= PARODD;
    } else {
        tio.c_cflag &= ~PARENB;
    }

    /* Stop bits */
    if (opts->stop_bits == 2) {
        tio.c_cflag |= CSTOPB;
    } else {
        tio.c_cflag &= ~CSTOPB;
    }

    /* Flow Control */
    if (opts->flow_control == 1) { /* hardware: RTS/CTS */
#ifdef CRTSCTS
        tio.c_cflag |= CRTSCTS;
#elif defined(CNEW_RTSCTS)
        tio.c_cflag |= CNEW_RTSCTS;
#endif
        tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    } else if (opts->flow_control == 2) { /* software: XON/XOFF */
#ifdef CRTSCTS
        tio.c_cflag &= ~CRTSCTS;
#endif
        tio.c_iflag |= (IXON | IXOFF);
    } else { /* none / off */
#ifdef CRTSCTS
        tio.c_cflag &= ~CRTSCTS;
#endif
        tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    }

    /* Raw input / output */
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tio.c_oflag &= ~OPOST;

    /* Read timeout (blocking read with 1 sec timeout) */
    tio.c_cc[VTIME] = 10;
    tio.c_cc[VMIN] = 0;

    /* Set Baud Rate */
    speed = get_posix_baud(opts->baud);
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    /* Apply attributes */
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        if (err_msg) {
            char buf[256];
            snprintf(buf, sizeof(buf), "tcsetattr failed: %s", strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        close(fd);
        free(port);
        return NULL;
    }

    /* Clear non-blocking flag for normal synchronous write */
    fcntl(fd, F_SETFL, 0);

    /* Set initial modem lines if configured */
    if (opts->rts >= 0) {
        solo_serial_set_rts(port, opts->rts);
    }
    if (opts->dtr >= 0) {
        solo_serial_set_dtr(port, opts->dtr);
    }

    return port;
}

int solo_serial_set_rts(solo_serial_t *port, int on)
{
#if defined(TIOCMGET) && defined(TIOCM_RTS)
    int status;
    if (port == NULL || port->fd < 0)
        return -1;
    if (ioctl(port->fd, TIOCMGET, &status) != 0)
        return -1;
    if (on)
        status |= TIOCM_RTS;
    else
        status &= ~TIOCM_RTS;
    return ioctl(port->fd, TIOCMSET, &status);
#else
    (void)port;
    (void)on;
    return 0;
#endif
}

int solo_serial_set_dtr(solo_serial_t *port, int on)
{
#if defined(TIOCMGET) && defined(TIOCM_DTR)
    int status;
    if (port == NULL || port->fd < 0)
        return -1;
    if (ioctl(port->fd, TIOCMGET, &status) != 0)
        return -1;
    if (on)
        status |= TIOCM_DTR;
    else
        status &= ~TIOCM_DTR;
    return ioctl(port->fd, TIOCMSET, &status);
#else
    (void)port;
    (void)on;
    return 0;
#endif
}

int solo_serial_write(solo_serial_t *port, const char *buf, size_t len)
{
    size_t total_written = 0;
    if (port == NULL || port->fd < 0 || buf == NULL) {
        return -1;
    }

    while (total_written < len) {
        ssize_t n = write(port->fd, buf + total_written, len - total_written);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        total_written += (size_t)n;
    }

    return (int)total_written;
}

int solo_serial_drain(solo_serial_t *port)
{
    if (port == NULL || port->fd < 0) {
        return -1;
    }
    return tcdrain(port->fd);
}

void solo_serial_close(solo_serial_t *port)
{
    if (port != NULL) {
        if (port->fd >= 0) {
            tcdrain(port->fd);
            tcsetattr(port->fd, TCSANOW, &port->orig_tio);
            close(port->fd);
            port->fd = -1;
        }
        free(port);
    }
}

#else
/* ISO C forbids an empty translation unit */
typedef int make_iso_compilers_happy_serial_posix;
#endif /* SOLO_IS_POSIX */
