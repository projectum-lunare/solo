/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L
#endif

#include <solo/platform.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

const char *solo_platform_os(void)
{
    return SOLO_OS_NAME;
}

const char *solo_platform_arch(void)
{
    return SOLO_ARCH_NAME;
}

int solo_platform_bits(void)
{
    return SOLO_BITS;
}

const char *solo_platform_endian(void)
{
    return SOLO_GET_ENDIAN_NAME();
}

const char *solo_strerror(solo_status_t status)
{
    switch (status) {
    case SOLO_OK:          return "Success";
    case SOLO_ERROR:       return "I/O or system error";
    case SOLO_AGAIN:       return "Operation would block or timeout expired";
    case SOLO_EOF:         return "End of stream / connection closed";
    case SOLO_UNSUPPORTED: return "Operation not supported on platform";
    default:               return "Unknown error";
    }
}

void solo_set_err(char *err_buf, size_t err_cap, const char *msg)
{
    if (err_buf != NULL && err_cap > 0) {
        if (msg != NULL) {
            size_t len = strlen(msg);
            if (len >= err_cap) {
                len = err_cap - 1;
            }
            memcpy(err_buf, msg, len);
            err_buf[len] = '\0';
        } else {
            err_buf[0] = '\0';
        }
    }
}

void solo_set_err_fmt(char *err_buf, size_t err_cap, const char *fmt, ...)
{
    if (err_buf != NULL && err_cap > 0) {
        if (fmt != NULL) {
            va_list ap;
            va_start(ap, fmt);
#if defined(_MSC_VER) && _MSC_VER < 1900
            _vsnprintf(err_buf, err_cap - 1, fmt, ap);
            err_buf[err_cap - 1] = '\0';
#else
            vsnprintf(err_buf, err_cap, fmt, ap);
            err_buf[err_cap - 1] = '\0';
#endif
            va_end(ap);
        } else {
            err_buf[0] = '\0';
        }
    }
}
