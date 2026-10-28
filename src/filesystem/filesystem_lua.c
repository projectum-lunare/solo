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

#include <solo/filesystem.h>

static int lfs_exists(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    lua_pushboolean(L, solo_fs_exists(path));
    return 1;
}

static const luaL_Reg fs_funcs[] = {
    {"exists", lfs_exists},
    {NULL, NULL}
};

LUAMOD_API int luaopen_solo_filesystem(lua_State *L);
LUAMOD_API int luaopen_solo_fs(lua_State *L);

LUAMOD_API int luaopen_solo_filesystem(lua_State *L)
{
    luaL_newlib(L, fs_funcs);
    return 1;
}

LUAMOD_API int luaopen_solo_fs(lua_State *L)
{
    return luaopen_solo_filesystem(L);
}
