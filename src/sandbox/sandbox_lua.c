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
    solo_sandbox_backend_t id = solo_sandbox_backend(&version);
    const char *name = solo_sandbox_backend_name(NULL);
    lua_pushstring(L, name);
    lua_pushinteger(L, version);
    lua_pushinteger(L, (lua_Integer)id);
    return 3;
}

static int lsandbox_is_supported(lua_State *L)
{
    int version = 0;
    solo_sandbox_backend_t id = solo_sandbox_backend(&version);
    lua_pushboolean(L, id != SOLO_SANDBOX_BACKEND_NONE);
    return 1;
}

static int parse_perms(const char *p)
{
    int flags = 0;
    if (p == NULL)
        return SOLO_SANDBOX_PERM_READ;
    if (strchr(p, 'r') || strchr(p, 'R'))
        flags |= SOLO_SANDBOX_PERM_READ;
    if (strchr(p, 'w') || strchr(p, 'W'))
        flags |= SOLO_SANDBOX_PERM_WRITE;
    if (strchr(p, 'x') || strchr(p, 'X'))
        flags |= SOLO_SANDBOX_PERM_EXEC;
    return flags ? flags : SOLO_SANDBOX_PERM_READ;
}

static int lsolo_sandbox_unveil(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    char err_buf[256];
    int perms;
    solo_sandbox_status_t res;

    err_buf[0] = '\0';
    if (lua_isinteger(L, 2)) {
        perms = (int)lua_tointeger(L, 2);
    } else {
        const char *perms_str = luaL_optstring(L, 2, "r");
        perms = parse_perms(perms_str);
    }

    res = solo_sandbox_unveil(path, perms, err_buf, sizeof(err_buf));
    if (res != SOLO_SANDBOX_OK) {
        lua_pushboolean(L, 0);
        if (err_buf[0] != '\0') {
            lua_pushstring(L, err_buf);
        } else {
            lua_pushstring(L, "sandbox unveil failed");
        }
        lua_pushinteger(L, (lua_Integer)res);
        return 3;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lsolo_sandbox_lock(lua_State *L)
{
    char err_buf[256];
    solo_sandbox_status_t res;

    err_buf[0] = '\0';
    res = solo_sandbox_lock(err_buf, sizeof(err_buf));
    if (res != SOLO_SANDBOX_OK) {
        lua_pushboolean(L, 0);
        if (err_buf[0] != '\0') {
            lua_pushstring(L, err_buf);
        } else {
            lua_pushstring(L, "sandbox lock failed");
        }
        lua_pushinteger(L, (lua_Integer)res);
        return 3;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lsolo_sandbox_pledge(lua_State *L)
{
    const char *promises = luaL_checkstring(L, 1);
    char err_buf[256];
    solo_sandbox_status_t res;

    err_buf[0] = '\0';
    res = solo_sandbox_pledge(promises, err_buf, sizeof(err_buf));
    if (res != SOLO_SANDBOX_OK) {
        lua_pushboolean(L, 0);
        if (err_buf[0] != '\0') {
            lua_pushstring(L, err_buf);
        } else {
            lua_pushstring(L, "sandbox pledge failed");
        }
        lua_pushinteger(L, (lua_Integer)res);
        return 3;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static const luaL_Reg sandbox_funcs[] = {
    {"backend",      lsandbox_backend     },
    {"get_backend",  lsandbox_backend     },
    {"is_supported", lsandbox_is_supported},
    {"unveil",       lsolo_sandbox_unveil },
    {"lock",         lsolo_sandbox_lock   },
    {"pledge",       lsolo_sandbox_pledge },
    {NULL,           NULL                 }
};

static void register_enums(lua_State *L)
{
    /* solo.sandbox.backend (callable table with enum constants) */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SANDBOX_BACKEND_NONE);     lua_setfield(L, -2, "NONE");
    lua_pushinteger(L, SOLO_SANDBOX_BACKEND_LANDLOCK); lua_setfield(L, -2, "LANDLOCK");
    lua_pushinteger(L, SOLO_SANDBOX_BACKEND_OPENBSD);  lua_setfield(L, -2, "OPENBSD");
    lua_pushinteger(L, SOLO_SANDBOX_BACKEND_CAPSICUM); lua_setfield(L, -2, "CAPSICUM");

    lua_newtable(L);
    lua_pushcfunction(L, lsandbox_backend);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    /* Also provide solo.sandbox.backends alias */
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, "backends");
    lua_setfield(L, -2, "backend");

    /* solo.sandbox.perm */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SANDBOX_PERM_NONE);  lua_setfield(L, -2, "NONE");
    lua_pushinteger(L, SOLO_SANDBOX_PERM_READ);  lua_setfield(L, -2, "READ");
    lua_pushinteger(L, SOLO_SANDBOX_PERM_WRITE); lua_setfield(L, -2, "WRITE");
    lua_pushinteger(L, SOLO_SANDBOX_PERM_EXEC);  lua_setfield(L, -2, "EXEC");
    lua_setfield(L, -2, "perm");

    /* solo.sandbox.status */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SANDBOX_OK);          lua_setfield(L, -2, "OK");
    lua_pushinteger(L, SOLO_SANDBOX_ERROR);       lua_setfield(L, -2, "ERROR");
    lua_pushinteger(L, SOLO_SANDBOX_UNSUPPORTED); lua_setfield(L, -2, "UNSUPPORTED");
    lua_setfield(L, -2, "status");

    /* Direct constants */
    lua_pushinteger(L, SOLO_SANDBOX_PERM_READ);   lua_setfield(L, -2, "READ");
    lua_pushinteger(L, SOLO_SANDBOX_PERM_WRITE);  lua_setfield(L, -2, "WRITE");
    lua_pushinteger(L, SOLO_SANDBOX_PERM_EXEC);   lua_setfield(L, -2, "EXEC");
    lua_pushinteger(L, SOLO_SANDBOX_OK);          lua_setfield(L, -2, "OK");
    lua_pushinteger(L, SOLO_SANDBOX_ERROR);       lua_setfield(L, -2, "ERROR");
    lua_pushinteger(L, SOLO_SANDBOX_UNSUPPORTED); lua_setfield(L, -2, "UNSUPPORTED");
}

int luaopen_solo_sandbox(lua_State *L);

int luaopen_solo_sandbox(lua_State *L)
{
    luaL_newlib(L, sandbox_funcs);
    register_enums(L);
    return 1;
}
