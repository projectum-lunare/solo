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

#include <solo/parallel.h>

#define PARALLEL_PORT_MT "dotstar.parallel.port"

typedef struct {
    solo_parallel_t *handle;
} lsolo_parallel_t;

static int lsolo_parallel_write(lua_State *L)
{
    lsolo_parallel_t *p = (lsolo_parallel_t *)luaL_checkudata(L, 1, PARALLEL_PORT_MT);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);
    int res;

    if (p->handle == NULL) {
        return luaL_error(L, "attempt to write to closed parallel port");
    }

    res = solo_parallel_write(p->handle, data, len);
    if (res < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "I/O error during parallel write");
        return 2;
    }

    lua_pushinteger(L, res);
    return 1;
}

static int lsolo_parallel_drain(lua_State *L)
{
    lsolo_parallel_t *p = (lsolo_parallel_t *)luaL_checkudata(L, 1, PARALLEL_PORT_MT);
    if (p->handle == NULL) {
        return luaL_error(L, "attempt to drain closed parallel port");
    }

    if (solo_parallel_drain(p->handle) != 0) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "parallel drain failed");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lsolo_parallel_close(lua_State *L)
{
    lsolo_parallel_t *p = (lsolo_parallel_t *)luaL_checkudata(L, 1, PARALLEL_PORT_MT);
    if (p->handle != NULL) {
        solo_parallel_close(p->handle);
        p->handle = NULL;
    }
    return 0;
}

static int lsolo_parallel_open(lua_State *L)
{
    const char *path = "LPT1";
    char *err_msg = NULL;
    solo_parallel_t *pp;
    lsolo_parallel_t *p;

    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "path");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 1, "port");
        }
        if (lua_isstring(L, -1)) {
            path = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
    } else if (!lua_isnoneornil(L, 1)) {
        path = luaL_checkstring(L, 1);
    }

    pp = solo_parallel_open(path, &err_msg);
    if (pp == NULL) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to open parallel device");
        }
        return 2;
    }

    p = (lsolo_parallel_t *)lua_newuserdatauv(L, sizeof(lsolo_parallel_t), 0);
    p->handle = pp;

    luaL_getmetatable(L, PARALLEL_PORT_MT);
    lua_setmetatable(L, -2);

    return 1;
}

static const luaL_Reg port_methods[] = {
    {"write",   lsolo_parallel_write},
    {"drain",   lsolo_parallel_drain},
    {"flush",   lsolo_parallel_drain},
    {"close",   lsolo_parallel_close},
    {"__close", lsolo_parallel_close},
    {"__gc",    lsolo_parallel_close},
    {NULL,      NULL           }
};

static const luaL_Reg parallel_funcs[] = {
    {"open", lsolo_parallel_open},
    {NULL,   NULL          }
};

static void register_parallel_enums(lua_State *L)
{
    /* status */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_PARALLEL_OK);
    lua_setfield(L, -2, "OK");
    lua_pushinteger(L, SOLO_PARALLEL_ERROR);
    lua_setfield(L, -2, "ERROR");
    lua_pushinteger(L, SOLO_PARALLEL_UNSUPPORTED);
    lua_setfield(L, -2, "UNSUPPORTED");
    lua_setfield(L, -2, "status");

    /* mode */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_PARALLEL_MODE_SPP);
    lua_setfield(L, -2, "SPP");
    lua_pushinteger(L, SOLO_PARALLEL_MODE_SPP);
    lua_setfield(L, -2, "CENTRONICS");
    lua_pushinteger(L, SOLO_PARALLEL_MODE_NIBBLE);
    lua_setfield(L, -2, "NIBBLE");
    lua_pushinteger(L, SOLO_PARALLEL_MODE_BYTE);
    lua_setfield(L, -2, "BYTE");
    lua_pushinteger(L, SOLO_PARALLEL_MODE_EPP);
    lua_setfield(L, -2, "EPP");
    lua_pushinteger(L, SOLO_PARALLEL_MODE_ECP);
    lua_setfield(L, -2, "ECP");
    lua_setfield(L, -2, "mode");

    /* pin */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_PARALLEL_PIN_BUSY);
    lua_setfield(L, -2, "BUSY");
    lua_pushinteger(L, SOLO_PARALLEL_PIN_ACK);
    lua_setfield(L, -2, "ACK");
    lua_pushinteger(L, SOLO_PARALLEL_PIN_PE);
    lua_setfield(L, -2, "PE");
    lua_pushinteger(L, SOLO_PARALLEL_PIN_PE);
    lua_setfield(L, -2, "PAPER_OUT");
    lua_pushinteger(L, SOLO_PARALLEL_PIN_SLCT);
    lua_setfield(L, -2, "SLCT");
    lua_pushinteger(L, SOLO_PARALLEL_PIN_SLCT);
    lua_setfield(L, -2, "ONLINE");
    lua_pushinteger(L, SOLO_PARALLEL_PIN_FAULT);
    lua_setfield(L, -2, "FAULT");
    lua_setfield(L, -2, "pin");

    /* port */
    lua_newtable(L);
    lua_pushstring(L, "LPT1");
    lua_setfield(L, -2, "LPT1");
    lua_pushstring(L, "LPT2");
    lua_setfield(L, -2, "LPT2");
    lua_pushstring(L, "LPT3");
    lua_setfield(L, -2, "LPT3");
    lua_setfield(L, -2, "port");
}

int luaopen_solo_parallel(lua_State *L)
{
    if (luaL_newmetatable(L, PARALLEL_PORT_MT)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, port_methods, 0);
    }
    lua_pop(L, 1);

    luaL_newlib(L, parallel_funcs);
    register_parallel_enums(L);
    return 1;
}
