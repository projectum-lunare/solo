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

solo_sandbox_backend_t solo_sandbox_backend(int *version_out)
{
    if (version_out != NULL) {
        *version_out = 0;
    }
    return SOLO_SANDBOX_BACKEND_NONE;
}

const char *solo_sandbox_backend_name(int *version_out)
{
    solo_sandbox_backend(version_out);
    return "none";
}

solo_bool_t solo_sandbox_is_supported(void)
{
    return SOLO_FALSE;
}

solo_sandbox_status_t solo_sandbox_unveil(const char *path, int permissions, char *err_buf, size_t err_cap)
{
    (void)path;
    (void)permissions;
    solo_set_err(err_buf, err_cap, "sandboxing is not supported on this platform");
    return SOLO_SANDBOX_UNSUPPORTED;
}

solo_sandbox_status_t solo_sandbox_lock(char *err_buf, size_t err_cap)
{
    solo_set_err(err_buf, err_cap, "sandboxing is not supported on this platform");
    return SOLO_SANDBOX_UNSUPPORTED;
}

solo_sandbox_status_t solo_sandbox_pledge(const char *promises, char *err_buf, size_t err_cap)
{
    (void)promises;
    solo_set_err(err_buf, err_cap, "sandboxing is not supported on this platform");
    return SOLO_SANDBOX_UNSUPPORTED;
}
