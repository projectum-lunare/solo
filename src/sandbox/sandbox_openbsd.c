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
#include <solo/sandbox.h>

#if defined(__OpenBSD__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

solo_sandbox_backend_t solo_sandbox_backend(int *version_out)
{
    if (version_out != NULL) {
        *version_out = 1;
    }
    return SOLO_SANDBOX_BACKEND_OPENBSD;
}

const char *solo_sandbox_backend_name(int *version_out)
{
    solo_sandbox_backend(version_out);
    return "openbsd";
}

solo_sandbox_status_t solo_sandbox_unveil(const char *path, int permissions, char *err_buf, size_t err_cap)
{
    char mode[8];
    int idx = 0;

    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (path == NULL || path[0] == '\0') {
        return SOLO_SANDBOX_OK;
    }

    if (permissions & SOLO_SANDBOX_PERM_READ) {
        mode[idx++] = 'r';
    }
    if (permissions & SOLO_SANDBOX_PERM_WRITE) {
        mode[idx++] = 'w';
        mode[idx++] = 'c';
    }
    if (permissions & SOLO_SANDBOX_PERM_EXEC) {
        mode[idx++] = 'x';
    }
    mode[idx] = '\0';

    if (unveil(path, mode) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "unveil(%.128s, %s) failed: %s", path, mode, strerror(errno));
        return SOLO_SANDBOX_ERROR;
    }
    return SOLO_SANDBOX_OK;
}

solo_sandbox_status_t solo_sandbox_lock(char *err_buf, size_t err_cap)
{
    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (unveil(NULL, NULL) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "unveil(NULL, NULL) failed: %s", strerror(errno));
        return SOLO_SANDBOX_ERROR;
    }
    return SOLO_SANDBOX_OK;
}

solo_sandbox_status_t solo_sandbox_pledge(const char *promises, char *err_buf, size_t err_cap)
{
    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (pledge(promises, NULL) != 0) {
        solo_set_err_fmt(err_buf, err_cap, "pledge(%.128s) failed: %s", promises ? promises : "", strerror(errno));
        return SOLO_SANDBOX_ERROR;
    }
    return SOLO_SANDBOX_OK;
}

#endif /* defined(__OpenBSD__) */
