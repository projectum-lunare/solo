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

const char *solo_sandbox_backend_name(int *version_out)
{
    if (version_out != NULL) {
        *version_out = 0;
    }
    return "none";
}

int solo_sandbox_unveil(const char *path, int permissions, char **err_msg)
{
    (void)path;
    (void)permissions;
    (void)err_msg;
    return 0;
}

int solo_sandbox_lock(char **err_msg)
{
    (void)err_msg;
    return 0;
}

int solo_sandbox_pledge(const char *promises, char **err_msg)
{
    (void)promises;
    (void)err_msg;
    return 0;
}
