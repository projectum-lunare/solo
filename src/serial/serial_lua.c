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

#include <solo/serial.h>

#define SERIAL_PORT_MT "dotstar.serial.port"

typedef struct {
    solo_serial_t *handle;
} lsolo_serial_t;

static int lsolo_serial_write(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);
    int res;

    if (p->handle == NULL) {
        return luaL_error(L, "attempt to write to closed serial port");
    }

    res = solo_serial_write(p->handle, data, len);
    if (res < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "I/O error during serial write");
        return 2;
    }

    lua_pushinteger(L, res);
    return 1;
}

static int lsolo_serial_drain(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    if (p->handle == NULL) {
        return luaL_error(L, "attempt to drain closed serial port");
    }

    if (solo_serial_drain(p->handle) != 0) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "serial drain failed");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lsolo_serial_close(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    if (p->handle != NULL) {
        solo_serial_close(p->handle);
        p->handle = NULL;
    }
    return 0;
}

static int lsolo_serial_set_rts(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    int on;
    if (p->handle == NULL) {
        return luaL_error(L, "attempt to use closed serial port");
    }
    on = lua_toboolean(L, 2) ? 1 : 0;
    if (solo_serial_set_rts(p->handle, on) != 0) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "failed to set RTS line");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lsolo_serial_set_dtr(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    int on;
    if (p->handle == NULL) {
        return luaL_error(L, "attempt to use closed serial port");
    }
    on = lua_toboolean(L, 2) ? 1 : 0;
    if (solo_serial_set_dtr(p->handle, on) != 0) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "failed to set DTR line");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

/*
 * native_serial.open(path, baud, data_bits, parity_code, stop_bits, flow_code, rts, dtr)
 * All parsing, default merging, and option normalization is performed in Lua.
 * C receives purely validated scalar primitives.
 */
static int lsolo_serial_open(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    solo_serial_opts_t opts;
    char *err_msg = NULL;
    solo_serial_t *sp;
    lsolo_serial_t *p;

    opts.baud = (int)luaL_optinteger(L, 2, 9600);
    opts.data_bits = (int)luaL_optinteger(L, 3, 8);
    opts.parity = (char)luaL_optinteger(L, 4, 'n');
    opts.stop_bits = (int)luaL_optinteger(L, 5, 1);
    opts.flow_control = (int)luaL_optinteger(L, 6, 1); /* 1: hardware */
    opts.rts = lua_isnoneornil(L, 7) ? 1 : (lua_toboolean(L, 7) ? 1 : 0);
    opts.dtr = lua_isnoneornil(L, 8) ? 1 : (lua_toboolean(L, 8) ? 1 : 0);

    sp = solo_serial_open(path, &opts, &err_msg);
    if (sp == NULL) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to open serial device");
        }
        return 2;
    }

    p = (lsolo_serial_t *)lua_newuserdatauv(L, sizeof(lsolo_serial_t), 0);
    p->handle = sp;

    luaL_getmetatable(L, SERIAL_PORT_MT);
    lua_setmetatable(L, -2);

    return 1;
}

static const luaL_Reg port_methods[] = {
    {"write",   lsolo_serial_write  },
    {"drain",   lsolo_serial_drain  },
    {"flush",   lsolo_serial_drain  },
    {"set_rts", lsolo_serial_set_rts},
    {"set_dtr", lsolo_serial_set_dtr},
    {"close",   lsolo_serial_close  },
    {"__close", lsolo_serial_close  },
    {"__gc",    lsolo_serial_close  },
    {NULL,      NULL           }
};

static const luaL_Reg serial_funcs[] = {
    {"open", lsolo_serial_open},
    {NULL,   NULL        }
};

int luaopen_solo_serial(lua_State *L)
{
    if (luaL_newmetatable(L, SERIAL_PORT_MT)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, port_methods, 0);
    }
    lua_pop(L, 1);

    luaL_newlib(L, serial_funcs);
    return 1;
}
