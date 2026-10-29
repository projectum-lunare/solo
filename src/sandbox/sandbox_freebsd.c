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

#if SOLO_IS_FREEBSD || defined(__FreeBSD__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>
#if defined(__FreeBSD_version) && __FreeBSD_version < 1000000
#include <sys/capability.h>
#else
#include <sys/capsicum.h>
#endif
#include <fcntl.h>
#include <unistd.h>

solo_sandbox_backend_t solo_sandbox_backend(int *version_out)
{
    if (version_out != NULL) {
#if defined(__FreeBSD_version)
        *version_out = (int)__FreeBSD_version;
#else
        *version_out = 1;
#endif
    }
    return SOLO_SANDBOX_BACKEND_CAPSICUM;
}

const char *solo_sandbox_backend_name(int *version_out)
{
    solo_sandbox_backend(version_out);
    return "capsicum";
}

solo_sandbox_status_t solo_sandbox_unveil(const char *path, int permissions, char **err_msg)
{
    /*
     * Under FreeBSD Capsicum, capability mode restricts path-based lookups after cap_enter().
     * In this unveil callback, we verify that the target path exists and is accessible
     * with the requested permissions before entering capability mode.
     */
    int mode = F_OK;

    if (path == NULL || path[0] == '\0') {
        return SOLO_SANDBOX_OK;
    }

    if (permissions & SOLO_SANDBOX_PERM_READ) {
        mode |= R_OK;
    }
    if (permissions & SOLO_SANDBOX_PERM_WRITE) {
        mode |= W_OK;
    }
    if (permissions & SOLO_SANDBOX_PERM_EXEC) {
        mode |= X_OK;
    }

    if (access(path, mode) != 0) {
        /* If write was requested on a non-existing file, check parent directory */
        if ((permissions & SOLO_SANDBOX_PERM_WRITE) && errno == ENOENT) {
            char dir_buf[1024];
            char *slash;
            size_t len = strlen(path);
            if (len >= sizeof(dir_buf))
                len = sizeof(dir_buf) - 1;
            memcpy(dir_buf, path, len);
            dir_buf[len] = '\0';
            slash = strrchr(dir_buf, '/');
            if (slash != NULL) {
                if (slash == dir_buf) {
                    *(slash + 1) = '\0';
                } else {
                    *slash = '\0';
                }
                if (access(dir_buf, W_OK) == 0) {
                    return SOLO_SANDBOX_OK;
                }
            } else {
                if (access(".", W_OK) == 0) {
                    return SOLO_SANDBOX_OK;
                }
            }
        }
        if (err_msg != NULL) {
            char buf[256];
            sprintf(buf, "access(%.128s) check failed: %s", path, strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        return SOLO_SANDBOX_ERROR;
    }

    return SOLO_SANDBOX_OK;
}

solo_sandbox_status_t solo_sandbox_lock(char **err_msg)
{
    if (cap_enter() != 0) {
        if (errno == ENOSYS) {
            if (err_msg != NULL) {
                *err_msg = safe_strdup("Capsicum capability mode is not supported by the running FreeBSD kernel");
            }
            return SOLO_SANDBOX_UNSUPPORTED;
        }
        if (err_msg != NULL) {
            char buf[128];
            sprintf(buf, "cap_enter() failed (%s)", strerror(errno));
            *err_msg = safe_strdup(buf);
        }
        return SOLO_SANDBOX_ERROR;
    }
    return SOLO_SANDBOX_OK;
}

solo_sandbox_status_t solo_sandbox_pledge(const char *promises, char **err_msg)
{
    (void)promises;
    if (err_msg != NULL) {
        *err_msg = safe_strdup("pledge is not supported on FreeBSD (Capsicum does not support pledge)");
    }
    return SOLO_SANDBOX_UNSUPPORTED;
}

#endif /* SOLO_IS_FREEBSD || defined(__FreeBSD__) */
