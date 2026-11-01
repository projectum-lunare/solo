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
#include <sys/types.h>
#include <sys/sysctl.h>
#if defined(__FreeBSD_version) && __FreeBSD_version < 1000000
#include <sys/capability.h>
#else
#include <sys/capsicum.h>
#endif
#include <fcntl.h>
#include <unistd.h>

static int s_cached_capsicum = -1;

/*
 * Probes whether Capsicum capability mode is actually supported by the running kernel.
 * 1. Checks sysctl kern.features.capsicum.
 * 2. Falls back to cap_getmode(&mode) which fails with ENOSYS if the kernel was built
 *    without CAPABILITY_MODE / CAPABILITIES options.
 */
static int probe_capsicum(void)
{
    int val = 0;
    size_t len = sizeof(val);
    unsigned int mode = 0;

    if (s_cached_capsicum != -1) {
        return s_cached_capsicum;
    }

    if (sysctlbyname("kern.features.capsicum", &val, &len, NULL, 0) == 0) {
        s_cached_capsicum = (val != 0) ? 1 : 0;
        return s_cached_capsicum;
    }

    errno = 0;
    if (cap_getmode(&mode) == 0) {
        s_cached_capsicum = 1;
    } else {
        s_cached_capsicum = 0;
    }

    return s_cached_capsicum;
}

solo_sandbox_backend_t solo_sandbox_backend(int *version_out)
{
    if (!probe_capsicum()) {
        if (version_out != NULL) {
            *version_out = 0;
        }
        return SOLO_SANDBOX_BACKEND_NONE;
    }

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
    solo_sandbox_backend_t b = solo_sandbox_backend(version_out);
    return (b == SOLO_SANDBOX_BACKEND_CAPSICUM) ? "capsicum" : "none";
}

solo_bool_t solo_sandbox_is_supported(void)
{
    return probe_capsicum() ? SOLO_TRUE : SOLO_FALSE;
}

solo_sandbox_status_t solo_sandbox_unveil(const char *path, int permissions, char *err_buf, size_t err_cap)
{
    (void)path;
    (void)permissions;
    solo_set_err(err_buf, err_cap, "unveil is not supported on FreeBSD (Capsicum operates on capability descriptors, not paths)");
    return SOLO_SANDBOX_UNSUPPORTED;
}

solo_sandbox_status_t solo_sandbox_lock(char *err_buf, size_t err_cap)
{
    if (err_buf != NULL && err_cap > 0) {
        err_buf[0] = '\0';
    }

    if (!probe_capsicum()) {
        solo_set_err(err_buf, err_cap, "Capsicum capability mode is not supported by the running FreeBSD kernel");
        return SOLO_SANDBOX_UNSUPPORTED;
    }

    if (cap_enter() != 0) {
        if (errno == ENOSYS) {
            solo_set_err(err_buf, err_cap, "Capsicum capability mode is not supported by the running FreeBSD kernel");
            return SOLO_SANDBOX_UNSUPPORTED;
        }
        solo_set_err_fmt(err_buf, err_cap, "cap_enter() failed (%s)", strerror(errno));
        return SOLO_SANDBOX_ERROR;
    }
    return SOLO_SANDBOX_OK;
}

solo_sandbox_status_t solo_sandbox_pledge(const char *promises, char *err_buf, size_t err_cap)
{
    (void)promises;
    solo_set_err(err_buf, err_cap, "pledge is not supported on FreeBSD (Capsicum does not support pledge)");
    return SOLO_SANDBOX_UNSUPPORTED;
}

#endif /* SOLO_IS_FREEBSD || defined(__FreeBSD__) */
