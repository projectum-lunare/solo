/*
 * solo - Physical I/O, System Foundations & Hardware Abstractions
 * Projectum Lunare <https://github.com/projectum-lunare/solo>
 * Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <solo/sandbox.h>

static int lsandbox_backend(lua_State *L)
{
    int version = 0;
    const char *name = solo_sandbox_backend_name(&version);
    lua_pushstring(L, name);
    lua_pushinteger(L, version);
    return 2;
}

static int lsandbox_is_supported(lua_State *L)
{
    int version = 0;
    const char *name = solo_sandbox_backend_name(&version);
    lua_pushboolean(L, strcmp(name, "none") != 0);
    return 1;
}

static int parse_perms(const char *p)
{
    int flags = 0;
    if (p == NULL)
        return SOLO_SANDBOX_READ;
    if (strchr(p, 'r') || strchr(p, 'R'))
        flags |= SOLO_SANDBOX_READ;
    if (strchr(p, 'w') || strchr(p, 'W'))
        flags |= SOLO_SANDBOX_WRITE;
    if (strchr(p, 'x') || strchr(p, 'X'))
        flags |= SOLO_SANDBOX_EXEC;
    return flags ? flags : SOLO_SANDBOX_READ;
}

static int lsolo_sandbox_unveil(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    const char *perms_str = luaL_optstring(L, 2, "r");
    char *err_msg = NULL;
    int perms = parse_perms(perms_str);
    int res = solo_sandbox_unveil(path, perms, &err_msg);

    if (res != 0) {
        lua_pushboolean(L, 0);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "sandbox unveil failed");
        }
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lsolo_sandbox_lock(lua_State *L)
{
    char *err_msg = NULL;
    int res = solo_sandbox_lock(&err_msg);
    if (res != 0) {
        lua_pushboolean(L, 0);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "sandbox lock failed");
        }
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lsolo_sandbox_pledge(lua_State *L)
{
    const char *promises = luaL_checkstring(L, 1);
    char *err_msg = NULL;
    int res = solo_sandbox_pledge(promises, &err_msg);
    if (res != 0) {
        lua_pushboolean(L, 0);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "sandbox pledge failed");
        }
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static const luaL_Reg sandbox_funcs[] = {
    {"backend",      lsandbox_backend     },
    {"is_supported", lsandbox_is_supported},
    {"unveil",       lsolo_sandbox_unveil      },
    {"lock",         lsolo_sandbox_lock        },
    {"pledge",       lsolo_sandbox_pledge      },
    {NULL,           NULL                 }
};

LUAMOD_API int luaopen_solo_sandbox(lua_State *L);

LUAMOD_API int luaopen_solo_sandbox(lua_State *L)
{
    luaL_newlib(L, sandbox_funcs);
    return 1;
}
