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

const char *solo_sandbox_backend_name(int *version_out)
{
    if (version_out != NULL) {
        *version_out = 1;
    }
    return "openbsd";
}

int solo_sandbox_unveil(const char *path, int permissions, char **err_msg)
{
    char mode[8];
    int idx = 0;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    if (permissions & SOLO_SANDBOX_READ) {
        mode[idx++] = 'r';
    }
    if (permissions & SOLO_SANDBOX_WRITE) {
        mode[idx++] = 'w';
        mode[idx++] = 'c';
    }
    if (permissions & SOLO_SANDBOX_EXEC) {
        mode[idx++] = 'x';
    }
    mode[idx] = '\0';

    if (unveil(path, mode) != 0) {
        if (err_msg != NULL) {
            char buf[256];
            sprintf(buf, "unveil(%.128s, %s) failed: %s", path, mode, strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        return -1;
    }
    return 0;
}

int solo_sandbox_lock(char **err_msg)
{
    if (unveil(NULL, NULL) != 0) {
        if (err_msg != NULL) {
            char buf[128];
            sprintf(buf, "unveil(NULL, NULL) failed: %s", strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        return -1;
    }
    return 0;
}

int solo_sandbox_pledge(const char *promises, char **err_msg)
{
    if (pledge(promises, NULL) != 0) {
        if (err_msg != NULL) {
            char buf[256];
            sprintf(buf, "pledge(%.128s) failed: %s", promises ? promises : "", strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        return -1;
    }
    return 0;
}

#endif /* defined(__OpenBSD__) */
