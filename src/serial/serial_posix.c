/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <solo/platform.h>

#if SOLO_IS_POSIX

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <solo/serial.h>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

struct serial_port {
    int fd;
    int timeout_ms;
    struct termios orig_tio;
};

static int get_posix_baud(int baud, speed_t *out_speed)
{
    switch (baud) {
    case 50:     *out_speed = B50; return 0;
    case 75:     *out_speed = B75; return 0;
    case 110:    *out_speed = B110; return 0;
    case 134:    *out_speed = B134; return 0;
    case 150:    *out_speed = B150; return 0;
    case 200:    *out_speed = B200; return 0;
    case 300:    *out_speed = B300; return 0;
    case 600:    *out_speed = B600; return 0;
    case 1200:   *out_speed = B1200; return 0;
    case 1800:   *out_speed = B1800; return 0;
    case 2400:   *out_speed = B2400; return 0;
    case 4800:   *out_speed = B4800; return 0;
    case 9600:   *out_speed = B9600; return 0;
    case 19200:  *out_speed = B19200; return 0;
    case 38400:  *out_speed = B38400; return 0;
#ifdef B57600
    case 57600:  *out_speed = B57600; return 0;
#endif
#ifdef B115200
    case 115200: *out_speed = B115200; return 0;
#endif
#ifdef B230400
    case 230400: *out_speed = B230400; return 0;
#endif
    default:
        return -1;
    }
}

solo_serial_t *solo_serial_open(const char *path, const solo_serial_opts_t *opts, char *err_buf, size_t err_cap)
{
    int fd;
    struct termios tio;
    solo_serial_t *port;
    speed_t speed;
    solo_serial_opts_t default_opts;

    const char *target = (path && path[0] != '\0') ? path : "/dev/ttyS0";

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (opts == NULL) {
        default_opts.baud = 9600;
        default_opts.data_bits = 8;
        default_opts.parity = 'n';
        default_opts.stop_bits = 1;
        default_opts.flow_control = 0; /* safe default: none */
        default_opts.rts = 1;
        default_opts.dtr = 1;
        default_opts.timeout_ms = -1; /* default: blocking indefinitely */
        opts = &default_opts;
    }

    /* Validate baud rate */
    if (get_posix_baud(opts->baud, &speed) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "unsupported baud rate %d", opts->baud);
        return NULL;
    }

    /* Open device non-blocking first to avoid hanging on DCD / carrier */
    fd = open(target, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        solo_set_err_fmt(err_buf, err_cap, "open(%s) failed: %s", target, strerror(errno));
        return NULL;
    }

    port = (solo_serial_t *)malloc(sizeof(solo_serial_t));
    if (port == NULL) {
        close(fd);
        solo_set_err(err_buf, err_cap, "Memory allocation failure");
        return NULL;
    }
    port->fd = fd;
    port->timeout_ms = opts->timeout_ms;

    if (tcgetattr(fd, &port->orig_tio) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "tcgetattr failed: %s", strerror(errno));
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

    /* Parity validation and configuration */
    if (opts->parity == 'e' || opts->parity == 'E') {
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
    } else if (opts->parity == 'o' || opts->parity == 'O') {
        tio.c_cflag |= PARENB;
        tio.c_cflag |= PARODD;
    } else if (opts->parity == 'n' || opts->parity == 'N' || opts->parity == '\0') {
        tio.c_cflag &= ~PARENB;
    } else if (opts->parity == 'm' || opts->parity == 'M') {
#ifdef CMSPAR
        tio.c_cflag |= (PARENB | CMSPAR | PARODD);
#else
        solo_set_err(err_buf, err_cap, "mark parity is unsupported on this platform");
        close(fd);
        free(port);
        return NULL;
#endif
    } else if (opts->parity == 's' || opts->parity == 'S') {
#ifdef CMSPAR
        tio.c_cflag |= (PARENB | CMSPAR);
        tio.c_cflag &= ~PARODD;
#else
        solo_set_err(err_buf, err_cap, "space parity is unsupported on this platform");
        close(fd);
        free(port);
        return NULL;
#endif
    } else {
        solo_set_err_fmt(err_buf, err_cap, "invalid parity option '%c'", opts->parity);
        close(fd);
        free(port);
        return NULL;
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

    /* Read timeout (inter-character timer 0, minimum chars 0 for non-blocking read) */
    tio.c_cc[VTIME] = 0;
    tio.c_cc[VMIN] = 0;

    /* Set Baud Rate */
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    /* Apply attributes */
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "tcsetattr failed: %s", strerror(errno));
        close(fd);
        free(port);
        return NULL;
    }

    /* Set blocking flag on descriptor (timeouts handled via poll) */
    fcntl(fd, F_SETFL, 0);

    /* Set initial modem lines if configured */
    if (opts->rts >= 0) {
        solo_serial_set_rts(port, (solo_serial_line_t)opts->rts);
    }
    if (opts->dtr >= 0) {
        solo_serial_set_dtr(port, (solo_serial_line_t)opts->dtr);
    }

    return port;
}

solo_serial_status_t solo_serial_set_rts(solo_serial_t *port, solo_serial_line_t state)
{
#if defined(TIOCMGET) && defined(TIOCM_RTS)
    int status;
    if (port == NULL || port->fd < 0)
        return SOLO_SERIAL_ERROR;
    if (ioctl(port->fd, TIOCMGET, &status) != 0)
        return SOLO_SERIAL_ERROR;
    if (state == SOLO_SERIAL_LINE_ON || (int)state == 1)
        status |= TIOCM_RTS;
    else
        status &= ~TIOCM_RTS;
    return (ioctl(port->fd, TIOCMSET, &status) == 0) ? SOLO_SERIAL_OK : SOLO_SERIAL_ERROR;
#else
    (void)port;
    (void)state;
    return SOLO_SERIAL_UNSUPPORTED;
#endif
}

solo_serial_status_t solo_serial_set_dtr(solo_serial_t *port, solo_serial_line_t state)
{
#if defined(TIOCMGET) && defined(TIOCM_DTR)
    int status;
    if (port == NULL || port->fd < 0)
        return SOLO_SERIAL_ERROR;
    if (ioctl(port->fd, TIOCMGET, &status) != 0)
        return SOLO_SERIAL_ERROR;
    if (state == SOLO_SERIAL_LINE_ON || (int)state == 1)
        status |= TIOCM_DTR;
    else
        status &= ~TIOCM_DTR;
    return (ioctl(port->fd, TIOCMSET, &status) == 0) ? SOLO_SERIAL_OK : SOLO_SERIAL_ERROR;
#else
    (void)port;
    (void)state;
    return SOLO_SERIAL_UNSUPPORTED;
#endif
}

solo_line_state_t solo_serial_get_cts(solo_serial_t *port)
{
#if defined(TIOCMGET) && defined(TIOCM_CTS)
    int status;
    if (port == NULL || port->fd < 0)
        return SOLO_LINE_ERROR;
    if (ioctl(port->fd, TIOCMGET, &status) != 0)
        return SOLO_LINE_ERROR;
    return (status & TIOCM_CTS) ? SOLO_LINE_HIGH : SOLO_LINE_LOW;
#else
    (void)port;
    return SOLO_LINE_ERROR;
#endif
}

solo_line_state_t solo_serial_get_dsr(solo_serial_t *port)
{
#if defined(TIOCMGET) && defined(TIOCM_DSR)
    int status;
    if (port == NULL || port->fd < 0)
        return SOLO_LINE_ERROR;
    if (ioctl(port->fd, TIOCMGET, &status) != 0)
        return SOLO_LINE_ERROR;
    return (status & TIOCM_DSR) ? SOLO_LINE_HIGH : SOLO_LINE_LOW;
#else
    (void)port;
    return SOLO_LINE_ERROR;
#endif
}

solo_line_state_t solo_serial_get_cd(solo_serial_t *port)
{
#if defined(TIOCMGET) && (defined(TIOCM_CD) || defined(TIOCM_CAR))
    int status;
    if (port == NULL || port->fd < 0)
        return SOLO_LINE_ERROR;
    if (ioctl(port->fd, TIOCMGET, &status) != 0)
        return SOLO_LINE_ERROR;
#if defined(TIOCM_CD)
    return (status & TIOCM_CD) ? SOLO_LINE_HIGH : SOLO_LINE_LOW;
#else
    return (status & TIOCM_CAR) ? SOLO_LINE_HIGH : SOLO_LINE_LOW;
#endif
#else
    (void)port;
    return SOLO_LINE_ERROR;
#endif
}

solo_line_state_t solo_serial_get_ri(solo_serial_t *port)
{
#if defined(TIOCMGET) && (defined(TIOCM_RI) || defined(TIOCM_RNG))
    int status;
    if (port == NULL || port->fd < 0)
        return SOLO_LINE_ERROR;
    if (ioctl(port->fd, TIOCMGET, &status) != 0)
        return SOLO_LINE_ERROR;
#if defined(TIOCM_RI)
    return (status & TIOCM_RI) ? SOLO_LINE_HIGH : SOLO_LINE_LOW;
#else
    return (status & TIOCM_RNG) ? SOLO_LINE_HIGH : SOLO_LINE_LOW;
#endif
#else
    (void)port;
    return SOLO_LINE_ERROR;
#endif
}

solo_serial_status_t solo_serial_set_timeout(solo_serial_t *port, int timeout_ms)
{
    if (port == NULL || port->fd < 0) {
        return SOLO_SERIAL_ERROR;
    }
    port->timeout_ms = timeout_ms;
    return SOLO_SERIAL_OK;
}

solo_status_t solo_serial_write(solo_serial_t *port, const char *buf, size_t len, size_t *written_bytes)
{
    size_t total_written = 0;
    long long deadline_ms = 0;
    int has_timeout = 0;

    if (written_bytes != NULL) {
        *written_bytes = 0;
    }

    if (port == NULL || port->fd < 0 || buf == NULL) {
        return SOLO_ERROR;
    }
    if (len == 0) {
        return SOLO_OK;
    }

    if (port->timeout_ms >= 0) {
        has_timeout = 1;
        if (port->timeout_ms > 0) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            deadline_ms = (long long)ts.tv_sec * 1000LL + (ts.tv_nsec / 1000000LL) + port->timeout_ms;
        }
    }

    while (total_written < len) {
        ssize_t n;
        if (has_timeout) {
            struct pollfd pfd;
            int pr;
            int poll_timeout = 0;

            if (port->timeout_ms > 0) {
                struct timespec ts;
                long long now_ms;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                now_ms = (long long)ts.tv_sec * 1000LL + (ts.tv_nsec / 1000000LL);
                if (now_ms >= deadline_ms) {
                    /* Total deadline expired */
                    if (written_bytes != NULL) {
                        *written_bytes = total_written;
                    }
                    return (total_written > 0) ? SOLO_OK : SOLO_AGAIN;
                }
                poll_timeout = (int)(deadline_ms - now_ms);
            } else {
                poll_timeout = 0; /* timeout_ms == 0: non-blocking check */
            }

            pfd.fd = port->fd;
            pfd.events = POLLOUT;
            pfd.revents = 0;

            do {
                pr = poll(&pfd, 1, poll_timeout);
            } while (pr < 0 && errno == EINTR);

            if (pr < 0) {
                if (written_bytes != NULL) {
                    *written_bytes = total_written;
                }
                return (total_written > 0) ? SOLO_OK : SOLO_ERROR;
            }
            if (pr == 0) {
                /* Poll timed out before descriptor became writable */
                if (written_bytes != NULL) {
                    *written_bytes = total_written;
                }
                return (total_written > 0) ? SOLO_OK : SOLO_AGAIN;
            }
        }

        n = write(port->fd, buf + total_written, len - total_written);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (has_timeout) {
                    continue;
                }
            }
            if (written_bytes != NULL) {
                *written_bytes = total_written;
            }
            return (total_written > 0) ? SOLO_OK : SOLO_ERROR;
        }
        if (n == 0) {
            break;
        }
        total_written += (size_t)n;
    }

    if (written_bytes != NULL) {
        *written_bytes = total_written;
    }
    return SOLO_OK;
}

solo_status_t solo_serial_read(solo_serial_t *port, char *buf, size_t max_len, size_t *read_bytes, int timeout_ms)
{
    ssize_t n;
    int eff_timeout;

    if (read_bytes != NULL) {
        *read_bytes = 0;
    }

    if (port == NULL || port->fd < 0 || buf == NULL || max_len == 0) {
        return SOLO_ERROR;
    }

    /*
     * If timeout_ms < 0: use port's configured timeout.
     * If caller passes timeout_ms >= 0: use explicitly supplied timeout.
     */
    eff_timeout = (timeout_ms >= 0) ? timeout_ms : port->timeout_ms;

    if (eff_timeout >= 0) {
        struct pollfd pfd;
        int pr;
        long long deadline_ms = 0;

        pfd.fd = port->fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        if (eff_timeout > 0) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            deadline_ms = (long long)ts.tv_sec * 1000LL + (ts.tv_nsec / 1000000LL) + eff_timeout;
        }

        while (1) {
            int current_timeout = 0;
            if (eff_timeout > 0) {
                struct timespec ts;
                long long now_ms;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                now_ms = (long long)ts.tv_sec * 1000LL + (ts.tv_nsec / 1000000LL);
                if (now_ms >= deadline_ms) {
                    return SOLO_AGAIN; /* Deadline expired: timeout */
                }
                current_timeout = (int)(deadline_ms - now_ms);
            } else {
                current_timeout = 0; /* Non-blocking poll */
            }

            pr = poll(&pfd, 1, current_timeout);
            if (pr < 0) {
                if (errno == EINTR) {
                    continue; /* Retry poll on signal interrupt until deadline */
                }
                return SOLO_ERROR; /* True poll error */
            }
            if (pr == 0) {
                return SOLO_AGAIN; /* Timeout: no data ready */
            }
            break; /* File descriptor ready for reading */
        }
    }

    do {
        n = read(port->fd, buf, max_len);
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SOLO_AGAIN;
        }
        return SOLO_ERROR;
    }

    if (read_bytes != NULL) {
        *read_bytes = (size_t)n;
    }
    return SOLO_OK;
}

solo_serial_status_t solo_serial_drain(solo_serial_t *port)
{
    if (port == NULL || port->fd < 0) {
        return SOLO_SERIAL_ERROR;
    }
    return (tcdrain(port->fd) == 0) ? SOLO_SERIAL_OK : SOLO_SERIAL_ERROR;
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
