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
    size_t written = 0;
    solo_status_t st;

    if (p->handle == NULL) {
        return luaL_error(L, "attempt to write to closed serial port");
    }
    if (len == 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    st = solo_serial_write(p->handle, data, len, &written);
    if (st == SOLO_AGAIN) {
        lua_pushnil(L);
        lua_pushliteral(L, "timeout");
        return 2;
    }
    if (st == SOLO_UNSUPPORTED) {
        lua_pushnil(L);
        lua_pushliteral(L, "operation unsupported on this platform");
        lua_pushinteger(L, SOLO_SERIAL_UNSUPPORTED);
        return 3;
    }
    if (st != SOLO_OK) {
        lua_pushnil(L);
        lua_pushliteral(L, "I/O error during serial write");
        return 2;
    }

    lua_pushinteger(L, (lua_Integer)written);
    return 1;
}

static int lsolo_serial_read(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    lua_Integer max_bytes = luaL_optinteger(L, 2, 4096);
    int timeout_ms = (int)luaL_optinteger(L, 3, -1);
    char *buf;
    size_t read_bytes = 0;
    solo_status_t st;

    if (p->handle == NULL) {
        return luaL_error(L, "attempt to read from closed serial port");
    }
    if (max_bytes <= 0) {
        lua_pushliteral(L, "");
        return 1;
    }

    buf = (char *)malloc((size_t)max_bytes);
    if (buf == NULL) {
        return luaL_error(L, "out of memory allocating serial read buffer");
    }

    st = solo_serial_read(p->handle, buf, (size_t)max_bytes, &read_bytes, timeout_ms);
    if (st == SOLO_AGAIN) {
        free(buf);
        lua_pushnil(L);
        lua_pushliteral(L, "timeout");
        return 2;
    }
    if (st == SOLO_UNSUPPORTED) {
        free(buf);
        lua_pushnil(L);
        lua_pushliteral(L, "operation unsupported on this platform");
        lua_pushinteger(L, SOLO_SERIAL_UNSUPPORTED);
        return 3;
    }
    if (st != SOLO_OK) {
        free(buf);
        lua_pushnil(L);
        lua_pushliteral(L, "I/O error during serial read");
        return 2;
    }

    lua_pushlstring(L, buf, read_bytes);
    free(buf);
    return 1;
}

static int lsolo_serial_set_timeout(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    int timeout_ms = (int)luaL_checkinteger(L, 2);
    solo_serial_status_t st;
    if (p->handle == NULL) {
        return luaL_error(L, "attempt to configure closed serial port");
    }
    st = solo_serial_set_timeout(p->handle, timeout_ms);
    if (st != SOLO_SERIAL_OK) {
        lua_pushboolean(L, 0);
        if (st == SOLO_SERIAL_UNSUPPORTED) {
            lua_pushliteral(L, "operation unsupported on this platform");
            lua_pushinteger(L, SOLO_SERIAL_UNSUPPORTED);
            return 3;
        }
        lua_pushliteral(L, "failed to set serial timeout");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lsolo_serial_get_cts(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    solo_line_state_t val;
    if (p->handle == NULL) return luaL_error(L, "attempt to use closed serial port");
    val = solo_serial_get_cts(p->handle);
    if (val == SOLO_LINE_ERROR) {
        lua_pushnil(L);
        lua_pushliteral(L, "unsupported or failed to read CTS");
        return 2;
    }
    lua_pushboolean(L, val == SOLO_LINE_HIGH);
    return 1;
}

static int lsolo_serial_get_dsr(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    solo_line_state_t val;
    if (p->handle == NULL) return luaL_error(L, "attempt to use closed serial port");
    val = solo_serial_get_dsr(p->handle);
    if (val == SOLO_LINE_ERROR) {
        lua_pushnil(L);
        lua_pushliteral(L, "unsupported or failed to read DSR");
        return 2;
    }
    lua_pushboolean(L, val == SOLO_LINE_HIGH);
    return 1;
}

static int lsolo_serial_get_cd(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    solo_line_state_t val;
    if (p->handle == NULL) return luaL_error(L, "attempt to use closed serial port");
    val = solo_serial_get_cd(p->handle);
    if (val == SOLO_LINE_ERROR) {
        lua_pushnil(L);
        lua_pushliteral(L, "unsupported or failed to read CD");
        return 2;
    }
    lua_pushboolean(L, val == SOLO_LINE_HIGH);
    return 1;
}

static int lsolo_serial_get_ri(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    solo_line_state_t val;
    if (p->handle == NULL) return luaL_error(L, "attempt to use closed serial port");
    val = solo_serial_get_ri(p->handle);
    if (val == SOLO_LINE_ERROR) {
        lua_pushnil(L);
        lua_pushliteral(L, "unsupported or failed to read RI");
        return 2;
    }
    lua_pushboolean(L, val == SOLO_LINE_HIGH);
    return 1;
}

static int lsolo_serial_drain(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    if (p->handle == NULL) {
        return luaL_error(L, "attempt to drain closed serial port");
    }

    if (solo_serial_drain(p->handle) != SOLO_SERIAL_OK) {
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

static solo_serial_line_t parse_line_state(lua_State *L, int idx)
{
    if (lua_isboolean(L, idx)) {
        return lua_toboolean(L, idx) ? SOLO_SERIAL_LINE_ON : SOLO_SERIAL_LINE_OFF;
    }
    if (lua_isnumber(L, idx)) {
        int v = (int)lua_tointeger(L, idx);
        return (v > 0) ? SOLO_SERIAL_LINE_ON : SOLO_SERIAL_LINE_OFF;
    }
    if (lua_isstring(L, idx)) {
        const char *s = lua_tostring(L, idx);
        if (s != NULL) {
            if (strcmp(s, "on") == 0 || strcmp(s, "ON") == 0 ||
                strcmp(s, "set") == 0 || strcmp(s, "SET") == 0 ||
                strcmp(s, "1") == 0 || strcmp(s, "high") == 0 || strcmp(s, "HIGH") == 0) {
                return SOLO_SERIAL_LINE_ON;
            }
            if (strcmp(s, "off") == 0 || strcmp(s, "OFF") == 0 ||
                strcmp(s, "clear") == 0 || strcmp(s, "CLEAR") == 0 ||
                strcmp(s, "0") == 0 || strcmp(s, "low") == 0 || strcmp(s, "LOW") == 0) {
                return SOLO_SERIAL_LINE_OFF;
            }
        }
    }
    return SOLO_SERIAL_LINE_OFF;
}

static int lsolo_serial_set_rts(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    solo_serial_line_t state;
    solo_serial_status_t st;

    if (p->handle == NULL) {
        return luaL_error(L, "attempt to use closed serial port");
    }
    state = parse_line_state(L, 2);
    st = solo_serial_set_rts(p->handle, state);
    if (st != SOLO_SERIAL_OK) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, (st == SOLO_SERIAL_UNSUPPORTED)
            ? "RTS control is not supported on this platform"
            : "failed to set RTS line");
        lua_pushinteger(L, st);
        return 3;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int lsolo_serial_set_dtr(lua_State *L)
{
    lsolo_serial_t *p = (lsolo_serial_t *)luaL_checkudata(L, 1, SERIAL_PORT_MT);
    solo_serial_line_t state;
    solo_serial_status_t st;

    if (p->handle == NULL) {
        return luaL_error(L, "attempt to use closed serial port");
    }
    state = parse_line_state(L, 2);
    st = solo_serial_set_dtr(p->handle, state);
    if (st != SOLO_SERIAL_OK) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, (st == SOLO_SERIAL_UNSUPPORTED)
            ? "DTR control is not supported on this platform"
            : "failed to set DTR line");
        lua_pushinteger(L, st);
        return 3;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static char parse_parity_value(lua_State *L, int idx, char default_val)
{
    if (lua_isnoneornil(L, idx)) {
        return default_val;
    }
    if (lua_isnumber(L, idx)) {
        return (char)lua_tointeger(L, idx);
    }
    if (lua_isstring(L, idx)) {
        const char *s = lua_tostring(L, idx);
        if (s == NULL || s[0] == '\0') {
            return default_val;
        }
        if (strcmp(s, "n") == 0 || strcmp(s, "none") == 0 || strcmp(s, "NONE") == 0) {
            return 'n';
        }
        if (strcmp(s, "e") == 0 || strcmp(s, "even") == 0 || strcmp(s, "EVEN") == 0) {
            return 'e';
        }
        if (strcmp(s, "o") == 0 || strcmp(s, "odd") == 0 || strcmp(s, "ODD") == 0) {
            return 'o';
        }
        if (strcmp(s, "m") == 0 || strcmp(s, "mark") == 0 || strcmp(s, "MARK") == 0) {
            return 'm';
        }
        if (strcmp(s, "s") == 0 || strcmp(s, "space") == 0 || strcmp(s, "SPACE") == 0) {
            return 's';
        }
        return s[0];
    }
    return default_val;
}

static int parse_flow_value(lua_State *L, int idx, int default_val)
{
    if (lua_isnoneornil(L, idx)) {
        return default_val;
    }
    if (lua_isnumber(L, idx)) {
        return (int)lua_tointeger(L, idx);
    }
    if (lua_isstring(L, idx)) {
        const char *s = lua_tostring(L, idx);
        if (s == NULL) return default_val;
        if (strcmp(s, "none") == 0 || strcmp(s, "NONE") == 0) {
            return SOLO_SERIAL_FLOW_NONE;
        }
        if (strcmp(s, "hardware") == 0 || strcmp(s, "rtscts") == 0 ||
            strcmp(s, "HARDWARE") == 0 || strcmp(s, "RTS_CTS") == 0) {
            return SOLO_SERIAL_FLOW_HARDWARE;
        }
        if (strcmp(s, "software") == 0 || strcmp(s, "xonxoff") == 0 ||
            strcmp(s, "SOFTWARE") == 0 || strcmp(s, "XON_XOFF") == 0) {
            return SOLO_SERIAL_FLOW_SOFTWARE;
        }
    }
    return default_val;
}

/*
 * native_serial.open(path, [opts_table | baud, data_bits, parity, stop_bits, flow, rts, dtr])
 * Supports both an ergonomic options table and positional arguments.
 */
static int lsolo_serial_open(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    solo_serial_opts_t opts;
    char err_buf[256];
    solo_serial_t *sp;
    lsolo_serial_t *p;

    err_buf[0] = '\0';

    opts.baud = 9600;
    opts.data_bits = 8;
    opts.parity = 'n';
    opts.stop_bits = 1;
    opts.flow_control = SOLO_SERIAL_FLOW_NONE;
    opts.rts = 1;
    opts.dtr = 1;
    opts.timeout_ms = -1;

    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "baud");
        if (!lua_isnil(L, -1)) {
            opts.baud = (int)luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "data_bits");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 2, "data");
        }
        if (!lua_isnil(L, -1)) {
            opts.data_bits = (int)luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "parity");
        if (!lua_isnil(L, -1)) {
            opts.parity = parse_parity_value(L, -1, 'n');
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "stop_bits");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 2, "stop");
        }
        if (!lua_isnil(L, -1)) {
            opts.stop_bits = (int)luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "flow");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 2, "flow_control");
        }
        if (!lua_isnil(L, -1)) {
            opts.flow_control = parse_flow_value(L, -1, SOLO_SERIAL_FLOW_NONE);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "rts");
        if (!lua_isnil(L, -1)) {
            if (lua_isboolean(L, -1)) {
                opts.rts = lua_toboolean(L, -1) ? 1 : 0;
            } else if (lua_isnumber(L, -1)) {
                opts.rts = (int)lua_tointeger(L, -1);
            }
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "dtr");
        if (!lua_isnil(L, -1)) {
            if (lua_isboolean(L, -1)) {
                opts.dtr = lua_toboolean(L, -1) ? 1 : 0;
            } else if (lua_isnumber(L, -1)) {
                opts.dtr = (int)lua_tointeger(L, -1);
            }
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "timeout");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_getfield(L, 2, "timeout_ms");
        }
        if (!lua_isnil(L, -1)) {
            opts.timeout_ms = (int)luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);
    } else {
        opts.baud = (int)luaL_optinteger(L, 2, 9600);
        opts.data_bits = (int)luaL_optinteger(L, 3, 8);
        opts.parity = parse_parity_value(L, 4, 'n');
        opts.stop_bits = (int)luaL_optinteger(L, 5, 1);
        opts.flow_control = parse_flow_value(L, 6, SOLO_SERIAL_FLOW_NONE);
        opts.rts = lua_isnoneornil(L, 7) ? 1 : (lua_toboolean(L, 7) ? 1 : 0);
        opts.dtr = lua_isnoneornil(L, 8) ? 1 : (lua_toboolean(L, 8) ? 1 : 0);
        opts.timeout_ms = (int)luaL_optinteger(L, 9, -1);
    }

    sp = solo_serial_open(path, &opts, err_buf, sizeof(err_buf));
    if (sp == NULL) {
        lua_pushnil(L);
        if (err_buf[0] != '\0') {
            lua_pushstring(L, err_buf);
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
    {"write",       lsolo_serial_write      },
    {"read",        lsolo_serial_read       },
    {"set_timeout", lsolo_serial_set_timeout},
    {"drain",       lsolo_serial_drain      },
    {"flush",       lsolo_serial_drain      },
    {"set_rts",     lsolo_serial_set_rts    },
    {"set_dtr",     lsolo_serial_set_dtr    },
    {"get_cts",     lsolo_serial_get_cts    },
    {"get_dsr",     lsolo_serial_get_dsr    },
    {"get_cd",      lsolo_serial_get_cd     },
    {"get_ri",      lsolo_serial_get_ri     },
    {"close",       lsolo_serial_close      },
    {"__close",     lsolo_serial_close      },
    {"__gc",        lsolo_serial_close      },
    {NULL,          NULL                    }
};

static const luaL_Reg serial_funcs[] = {
    {"open", lsolo_serial_open},
    {NULL,   NULL        }
};

static void register_serial_enums(lua_State *L)
{
    /* flow */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SERIAL_FLOW_NONE);
    lua_setfield(L, -2, "NONE");
    lua_pushinteger(L, SOLO_SERIAL_FLOW_HARDWARE);
    lua_setfield(L, -2, "HARDWARE");
    lua_pushinteger(L, SOLO_SERIAL_FLOW_SOFTWARE);
    lua_setfield(L, -2, "SOFTWARE");
    lua_pushinteger(L, SOLO_SERIAL_FLOW_HARDWARE);
    lua_setfield(L, -2, "RTS_CTS");
    lua_pushinteger(L, SOLO_SERIAL_FLOW_SOFTWARE);
    lua_setfield(L, -2, "XON_XOFF");
    lua_setfield(L, -2, "flow");

    /* parity */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SERIAL_PARITY_NONE);
    lua_setfield(L, -2, "NONE");
    lua_pushinteger(L, SOLO_SERIAL_PARITY_EVEN);
    lua_setfield(L, -2, "EVEN");
    lua_pushinteger(L, SOLO_SERIAL_PARITY_ODD);
    lua_setfield(L, -2, "ODD");
    lua_pushinteger(L, SOLO_SERIAL_PARITY_MARK);
    lua_setfield(L, -2, "MARK");
    lua_pushinteger(L, SOLO_SERIAL_PARITY_SPACE);
    lua_setfield(L, -2, "SPACE");
    lua_setfield(L, -2, "parity");

    /* stop_bits & stop */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SERIAL_STOP_1);
    lua_setfield(L, -2, "ONE");
    lua_pushinteger(L, SOLO_SERIAL_STOP_2);
    lua_setfield(L, -2, "TWO");
    lua_pushinteger(L, SOLO_SERIAL_STOP_1);
    lua_setfield(L, -2, "STOP_1");
    lua_pushinteger(L, SOLO_SERIAL_STOP_2);
    lua_setfield(L, -2, "STOP_2");
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, "stop_bits");
    lua_setfield(L, -2, "stop");

    /* data_bits & data */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SERIAL_DATA_5);
    lua_setfield(L, -2, "FIVE");
    lua_pushinteger(L, SOLO_SERIAL_DATA_6);
    lua_setfield(L, -2, "SIX");
    lua_pushinteger(L, SOLO_SERIAL_DATA_7);
    lua_setfield(L, -2, "SEVEN");
    lua_pushinteger(L, SOLO_SERIAL_DATA_8);
    lua_setfield(L, -2, "EIGHT");
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, "data_bits");
    lua_setfield(L, -2, "data");

    /* baud */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SERIAL_BAUD_300);
    lua_setfield(L, -2, "B300");
    lua_pushinteger(L, SOLO_SERIAL_BAUD_1200);
    lua_setfield(L, -2, "B1200");
    lua_pushinteger(L, SOLO_SERIAL_BAUD_2400);
    lua_setfield(L, -2, "B2400");
    lua_pushinteger(L, SOLO_SERIAL_BAUD_4800);
    lua_setfield(L, -2, "B4800");
    lua_pushinteger(L, SOLO_SERIAL_BAUD_9600);
    lua_setfield(L, -2, "B9600");
    lua_pushinteger(L, SOLO_SERIAL_BAUD_19200);
    lua_setfield(L, -2, "B19200");
    lua_pushinteger(L, SOLO_SERIAL_BAUD_38400);
    lua_setfield(L, -2, "B38400");
    lua_pushinteger(L, SOLO_SERIAL_BAUD_57600);
    lua_setfield(L, -2, "B57600");
    lua_pushinteger(L, SOLO_SERIAL_BAUD_115200);
    lua_setfield(L, -2, "B115200");
    lua_setfield(L, -2, "baud");

    /* line */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SERIAL_LINE_IGNORE);
    lua_setfield(L, -2, "IGNORE");
    lua_pushinteger(L, SOLO_SERIAL_LINE_OFF);
    lua_setfield(L, -2, "OFF");
    lua_pushinteger(L, SOLO_SERIAL_LINE_ON);
    lua_setfield(L, -2, "ON");
    lua_pushinteger(L, SOLO_LINE_LOW);
    lua_setfield(L, -2, "LOW");
    lua_pushinteger(L, SOLO_LINE_HIGH);
    lua_setfield(L, -2, "HIGH");
    lua_pushinteger(L, SOLO_LINE_ERROR);
    lua_setfield(L, -2, "ERROR");
    lua_setfield(L, -2, "line");

    /* status */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SERIAL_OK);
    lua_setfield(L, -2, "OK");
    lua_pushinteger(L, SOLO_SERIAL_ERROR);
    lua_setfield(L, -2, "ERROR");
    lua_pushinteger(L, SOLO_SERIAL_UNSUPPORTED);
    lua_setfield(L, -2, "UNSUPPORTED");
    lua_setfield(L, -2, "status");
}

int luaopen_solo_serial(lua_State *L)
{
    if (luaL_newmetatable(L, SERIAL_PORT_MT)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, port_methods, 0);
    }
    lua_pop(L, 1);

    luaL_newlib(L, serial_funcs);
    register_serial_enums(L);
    return 1;
}
