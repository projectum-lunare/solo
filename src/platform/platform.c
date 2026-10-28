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
