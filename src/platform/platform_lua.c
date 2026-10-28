/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <solo/platform.h>

#define SET_BOOL(key, val)                                                                         \
    do {                                                                                           \
        lua_pushboolean(L, (val) ? 1 : 0);                                                         \
        lua_setfield(L, -2, (key));                                                                \
    } while (0)


int luaopen_solo_platform(lua_State *L);

int luaopen_solo_platform(lua_State *L)
{
    lua_newtable(L);

    lua_pushstring(L, SOLO_OS_NAME);
    lua_setfield(L, -2, "os");
    lua_pushstring(L, SOLO_ARCH_NAME);
    lua_setfield(L, -2, "arch");
    lua_pushinteger(L, (lua_Integer)SOLO_BITS);
    lua_setfield(L, -2, "bits");
    lua_pushstring(L, SOLO_GET_ENDIAN_NAME());
    lua_setfield(L, -2, "endian");

    SET_BOOL("is_64bit", SOLO_IS_64BIT);
    SET_BOOL("is_32bit", SOLO_IS_32BIT);
    SET_BOOL("is_16bit", SOLO_IS_16BIT);

    SET_BOOL("is_little_endian", SOLO_IS_LITTLE_ENDIAN);
    SET_BOOL("is_big_endian", SOLO_IS_BIG_ENDIAN);

    SET_BOOL("is_posix", SOLO_IS_POSIX);
    SET_BOOL("is_windows", SOLO_IS_WINDOWS);
    SET_BOOL("is_windows_nt", SOLO_IS_WINDOWS_NT);
    SET_BOOL("is_windows_ce", SOLO_IS_WINDOWS_CE);
    SET_BOOL("is_amiga", SOLO_IS_AMIGA);
    SET_BOOL("is_amiga_classic", SOLO_IS_AMIGA_CLASSIC);
    SET_BOOL("is_aros", SOLO_IS_AROS);
    SET_BOOL("is_amigaos4", SOLO_IS_AMIGAOS4);
    SET_BOOL("is_linux", SOLO_IS_LINUX);
    SET_BOOL("is_darwin", SOLO_IS_DARWIN);
    SET_BOOL("is_bsd", SOLO_IS_BSD);
    SET_BOOL("is_haiku", SOLO_IS_HAIKU);
    SET_BOOL("is_beos", SOLO_IS_BEOS);
    SET_BOOL("is_plan9", SOLO_IS_PLAN9);
    SET_BOOL("is_kolibri", SOLO_IS_KOLIBRI);
    SET_BOOL("is_minix", SOLO_IS_MINIX);
    SET_BOOL("is_dos", SOLO_IS_DOS);
    SET_BOOL("is_os2", SOLO_IS_OS2);
    SET_BOOL("is_atarist", SOLO_IS_ATARIST);
    SET_BOOL("is_mac68k", SOLO_IS_MAC68K);
    SET_BOOL("is_x68k", SOLO_IS_X68K);
    SET_BOOL("is_wasi", SOLO_IS_WASI);

    return 1;
}
