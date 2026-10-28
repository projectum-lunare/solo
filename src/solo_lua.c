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

#include <solo/solo.h>

#ifndef SOLO_NO_PLATFORM
int luaopen_solo_platform(lua_State *L);
#endif

#ifndef SOLO_NO_FILESYSTEM
int luaopen_solo_filesystem(lua_State *L);
int luaopen_solo_fs(lua_State *L);
#endif

#ifndef SOLO_NO_PARALLEL
int luaopen_solo_parallel(lua_State *L);
#endif

#ifndef SOLO_NO_SERIAL
int luaopen_solo_serial(lua_State *L);
#endif

#ifndef SOLO_NO_SOCKET
int luaopen_solo_socket(lua_State *L);
#endif

#ifndef SOLO_NO_SANDBOX
int luaopen_solo_sandbox(lua_State *L);
#endif

static int lsolo_version(lua_State *L)
{
    lua_pushstring(L, solo_version());
    return 1;
}

static const struct luaL_Reg solo_funcs[] = {
    {"version", lsolo_version},
    {NULL, NULL}
};

static void load_submodule(lua_State *L, const char *name, int (*openf)(lua_State *))
{
    lua_pushcfunction(L, openf);
    lua_pushstring(L, name);
    lua_call(L, 1, 1);

    lua_getglobal(L, "package");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "loaded");
        if (lua_istable(L, -1)) {
            lua_pushvalue(L, -3);
            lua_setfield(L, -2, name);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

int luaopen_solo(lua_State *L)
{
#if LUA_VERSION_NUM >= 502
    luaL_newlib(L, solo_funcs);
#else
    lua_newtable(L);
    luaL_register(L, NULL, solo_funcs);
#endif

#ifndef SOLO_NO_PLATFORM
    load_submodule(L, "solo.platform", luaopen_solo_platform);
    lua_setfield(L, -2, "platform");
#endif

#ifndef SOLO_NO_FILESYSTEM
    load_submodule(L, "solo.filesystem", luaopen_solo_filesystem);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, "filesystem");
    lua_setfield(L, -2, "fs");

    lua_getglobal(L, "package");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "loaded");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "solo.filesystem");
            lua_setfield(L, -2, "solo.fs");
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
#endif

#ifndef SOLO_NO_PARALLEL
    load_submodule(L, "solo.parallel", luaopen_solo_parallel);
    lua_setfield(L, -2, "parallel");
#endif

#ifndef SOLO_NO_SERIAL
    load_submodule(L, "solo.serial", luaopen_solo_serial);
    lua_setfield(L, -2, "serial");
#endif

#ifndef SOLO_NO_SOCKET
    load_submodule(L, "solo.socket", luaopen_solo_socket);
    lua_setfield(L, -2, "socket");
#endif

#ifndef SOLO_NO_SANDBOX
    load_submodule(L, "solo.sandbox", luaopen_solo_sandbox);
    lua_setfield(L, -2, "sandbox");
#endif

    return 1;
}
