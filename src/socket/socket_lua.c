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

#include <solo/socket.h>

#define SOCKET_MT "solo.socket"

typedef struct {
    solo_socket_t *handle;
} lsocket_t;

static int lsocket_write(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);
    int res;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to write to closed socket");
    }

    res = solo_socket_write(s->handle, data, len);
    if (res < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "socket write failed");
        return 2;
    }

    lua_pushinteger(L, res);
    return 1;
}

static int lsocket_read(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    lua_Integer max_len = luaL_optinteger(L, 2, 4096);
    char *buf;
    int res;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to read from closed socket");
    }

    if (max_len <= 0) {
        lua_pushstring(L, "");
        return 1;
    }

    buf = (char *)malloc((size_t)max_len);
    if (buf == NULL) {
        return luaL_error(L, "out of memory in socket read");
    }

    res = solo_socket_read(s->handle, buf, (size_t)max_len);
    if (res < 0) {
        free(buf);
        lua_pushnil(L);
        lua_pushstring(L, "socket read error");
        return 2;
    }

    if (res == 0) {
        free(buf);
        lua_pushnil(L);
        lua_pushstring(L, "closed");
        return 2;
    }

    lua_pushlstring(L, buf, (size_t)res);
    free(buf);
    return 1;
}

static int lsocket_accept(lua_State *L)
{
    lsocket_t *server = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    char client_ip[64];
    int client_port = 0;
    char *err_msg = NULL;
    solo_socket_t *client_handle;
    lsocket_t *client_sock;

    if (server->handle == NULL) {
        return luaL_error(L, "attempt to accept on closed server socket");
    }

    client_handle =
        solo_socket_accept(server->handle, client_ip, sizeof(client_ip), &client_port, &err_msg);
    if (client_handle == NULL) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "socket accept failed");
        }
        return 2;
    }

    client_sock = (lsocket_t *)lua_newuserdatauv(L, sizeof(lsocket_t), 0);
    client_sock->handle = client_handle;

    luaL_getmetatable(L, SOCKET_MT);
    lua_setmetatable(L, -2);

    lua_pushstring(L, client_ip);
    lua_pushinteger(L, client_port);
    return 3;
}

static int lsocket_close(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    if (s->handle != NULL) {
        solo_socket_close(s->handle);
        s->handle = NULL;
    }
    return 0;
}

static int lsocket_connect(lua_State *L)
{
    const char *host = luaL_checkstring(L, 1);
    int port = (int)luaL_checkinteger(L, 2);
    char *err_msg = NULL;
    solo_socket_t *handle;
    lsocket_t *s;

    handle = solo_socket_connect(host, port, &err_msg);
    if (handle == NULL) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to connect");
        }
        return 2;
    }

    s = (lsocket_t *)lua_newuserdatauv(L, sizeof(lsocket_t), 0);
    s->handle = handle;

    luaL_getmetatable(L, SOCKET_MT);
    lua_setmetatable(L, -2);

    return 1;
}

static int lsocket_bind(lua_State *L)
{
    const char *host = luaL_optstring(L, 1, "0.0.0.0");
    int port = (int)luaL_optinteger(L, 2, 9100);
    int backlog = (int)luaL_optinteger(L, 3, 5);
    char *err_msg = NULL;
    solo_socket_t *handle;
    lsocket_t *s;

    handle = solo_socket_bind(host, port, backlog, &err_msg);
    if (handle == NULL) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to bind socket");
        }
        return 2;
    }

    s = (lsocket_t *)lua_newuserdatauv(L, sizeof(lsocket_t), 0);
    s->handle = handle;

    luaL_getmetatable(L, SOCKET_MT);
    lua_setmetatable(L, -2);

    return 1;
}

static const luaL_Reg socket_methods[] = {
    {"write",   lsocket_write },
    {"read",    lsocket_read  },
    {"accept",  lsocket_accept},
    {"close",   lsocket_close },
    {"__close", lsocket_close },
    {"__gc",    lsocket_close },
    {NULL,      NULL          }
};

static const luaL_Reg socket_funcs[] = {
    {"connect", lsocket_connect},
    {"bind",    lsocket_bind   },
    {NULL,      NULL           }
};

int luaopen_solo_socket(lua_State *L)
{
    if (luaL_newmetatable(L, SOCKET_MT)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, socket_methods, 0);
    }
    lua_pop(L, 1);

    luaL_newlib(L, socket_funcs);
    return 1;
}
