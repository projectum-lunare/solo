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

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <solo/socket.h>

#define SOCKET_MT "solo.socket"

#if LUA_VERSION_NUM < 504
#define lua_newuserdatauv(L, sz, nuv) ((void)(nuv), lua_newuserdata(L, sz))
#endif

#if LUA_VERSION_NUM < 502
#define luaL_newlib(L, l) (lua_newtable(L), luaL_register(L, NULL, l))
#endif

typedef struct {
    solo_socket_t *handle;
} lsocket_t;

static lsocket_t *push_socket_udata(lua_State *L, solo_socket_t *handle)
{
    lsocket_t *s = (lsocket_t *)lua_newuserdatauv(L, sizeof(lsocket_t), 0);
    s->handle = handle;
    luaL_getmetatable(L, SOCKET_MT);
    lua_setmetatable(L, -2);
    return s;
}

static solo_socket_mode_t parse_mode(lua_State *L, int idx, solo_socket_mode_t default_mode)
{
    if (lua_isnoneornil(L, idx)) {
        return default_mode;
    }
    if (lua_isboolean(L, idx)) {
        return lua_toboolean(L, idx) ? SOLO_SOCK_BLOCKING : SOLO_SOCK_NONBLOCKING;
    }
    if (lua_isnumber(L, idx)) {
        int m = (int)lua_tointeger(L, idx);
        return (m == SOLO_SOCK_NONBLOCKING) ? SOLO_SOCK_NONBLOCKING : SOLO_SOCK_BLOCKING;
    }
    if (lua_isstring(L, idx)) {
        const char *s = lua_tostring(L, idx);
        if (strcmp(s, "nonblocking") == 0 || strcmp(s, "async") == 0) {
            return SOLO_SOCK_NONBLOCKING;
        }
        return SOLO_SOCK_BLOCKING;
    }
    return default_mode;
}

static solo_socket_type_t parse_type(lua_State *L, int idx)
{
    if (lua_isnumber(L, idx)) {
        int t = (int)lua_tointeger(L, idx);
        return (t == SOLO_SOCK_UDP) ? SOLO_SOCK_UDP : SOLO_SOCK_TCP;
    }
    if (lua_isstring(L, idx)) {
        const char *s = lua_tostring(L, idx);
        if (strcmp(s, "udp") == 0 || strcmp(s, "dgram") == 0) {
            return SOLO_SOCK_UDP;
        }
    }
    return SOLO_SOCK_TCP;
}

/* =============================================================================
 * Socket Methods
 * ============================================================================= */

static int lsocket_send(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);
    int res;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to send on closed socket");
    }

    res = solo_socket_send(s->handle, data, len);
    if (res == SOLO_SOCK_AGAIN) {
        lua_pushnil(L);
        lua_pushstring(L, "again");
        return 2;
    }
    if (res < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "send failed");
        return 2;
    }

    lua_pushinteger(L, res);
    return 1;
}

static int lsocket_recv(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    lua_Integer max_len = luaL_optinteger(L, 2, 4096);
    char *buf;
    int res;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to recv on closed socket");
    }

    if (max_len <= 0) {
        lua_pushstring(L, "");
        return 1;
    }

    buf = (char *)malloc((size_t)max_len);
    if (buf == NULL) {
        return luaL_error(L, "out of memory in socket recv");
    }

    res = solo_socket_recv(s->handle, buf, (size_t)max_len);
    if (res == SOLO_SOCK_AGAIN) {
        free(buf);
        lua_pushnil(L);
        lua_pushstring(L, "again");
        return 2;
    }

    if (res < 0) {
        free(buf);
        lua_pushnil(L);
        lua_pushstring(L, "recv error");
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

static int lsocket_sendto(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);
    const char *host = luaL_checkstring(L, 3);
    int port = (int)luaL_checkinteger(L, 4);
    int res;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to sendto on closed socket");
    }

    res = solo_socket_sendto(s->handle, data, len, host, port);
    if (res == SOLO_SOCK_AGAIN) {
        lua_pushnil(L);
        lua_pushstring(L, "again");
        return 2;
    }
    if (res < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "sendto failed");
        return 2;
    }

    lua_pushinteger(L, res);
    return 1;
}

static int lsocket_recvfrom(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    lua_Integer max_len = luaL_optinteger(L, 2, 4096);
    char src_ip[64];
    int src_port = 0;
    char *buf;
    int res;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to recvfrom on closed socket");
    }

    if (max_len <= 0) {
        lua_pushstring(L, "");
        lua_pushstring(L, "");
        lua_pushinteger(L, 0);
        return 3;
    }

    buf = (char *)malloc((size_t)max_len);
    if (buf == NULL) {
        return luaL_error(L, "out of memory in socket recvfrom");
    }

    res = solo_socket_recvfrom(s->handle, buf, (size_t)max_len, src_ip, sizeof(src_ip), &src_port);
    if (res == SOLO_SOCK_AGAIN) {
        free(buf);
        lua_pushnil(L);
        lua_pushstring(L, "again");
        return 2;
    }

    if (res < 0) {
        free(buf);
        lua_pushnil(L);
        lua_pushstring(L, "recvfrom error");
        return 2;
    }

    lua_pushlstring(L, buf, (size_t)res);
    free(buf);
    lua_pushstring(L, src_ip);
    lua_pushinteger(L, src_port);
    return 3;
}

static int lsocket_bind_sock(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    const char *host = luaL_optstring(L, 2, "0.0.0.0");
    int port = (int)luaL_optinteger(L, 3, 9100);
    int backlog = (int)luaL_optinteger(L, 4, 128);
    char *err_msg = NULL;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to bind closed socket");
    }

    if (solo_socket_bind_sock(s->handle, host, port, backlog, &err_msg) != SOLO_SOCK_OK) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "bind failed");
        }
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lsocket_connect_sock(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    const char *host = luaL_checkstring(L, 2);
    int port = (int)luaL_checkinteger(L, 3);
    char *err_msg = NULL;
    int res;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to connect closed socket");
    }

    res = solo_socket_connect_sock(s->handle, host, port, &err_msg);
    if (res == SOLO_SOCK_AGAIN) {
        lua_pushnil(L);
        lua_pushstring(L, "again");
        return 2;
    }
    if (res != SOLO_SOCK_OK) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to connect");
        }
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lsocket_accept(lua_State *L)
{
    lsocket_t *server = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    char client_ip[64];
    int client_port = 0;
    char *err_msg = NULL;
    solo_socket_t *client_handle;

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
            lua_pushstring(L, "again");
        }
        return 2;
    }

    push_socket_udata(L, client_handle);
    lua_pushstring(L, client_ip);
    lua_pushinteger(L, client_port);
    return 3;
}

static int lsocket_set_mode(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    solo_socket_mode_t mode = parse_mode(L, 2, SOLO_SOCK_BLOCKING);
    char *err_msg = NULL;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to set mode on closed socket");
    }

    if (solo_socket_set_mode(s->handle, mode, &err_msg) != SOLO_SOCK_OK) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to set socket mode");
        }
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lsocket_set_blocking(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    int blk = lua_toboolean(L, 2);
    char *err_msg = NULL;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to set mode on closed socket");
    }

    if (solo_socket_set_mode(s->handle, blk ? SOLO_SOCK_BLOCKING : SOLO_SOCK_NONBLOCKING, &err_msg) != SOLO_SOCK_OK) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to set blocking mode");
        }
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lsocket_get_mode(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    if (s->handle == NULL) {
        return luaL_error(L, "attempt to get mode on closed socket");
    }
    lua_pushinteger(L, (lua_Integer)solo_socket_get_mode(s->handle));
    return 1;
}

static int lsocket_is_blocking(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    if (s->handle == NULL) {
        return luaL_error(L, "attempt to check blocking on closed socket");
    }
    lua_pushboolean(L, solo_socket_get_mode(s->handle) == SOLO_SOCK_BLOCKING);
    return 1;
}

static int lsocket_set_timeout(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    int timeout_ms = (int)luaL_checkinteger(L, 2);
    char *err_msg = NULL;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to set timeout on closed socket");
    }

    if (solo_socket_set_timeout(s->handle, timeout_ms, &err_msg) != SOLO_SOCK_OK) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to set socket timeout");
        }
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lsocket_wait(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    int events = SOLO_SOCK_POLL_READ;
    int timeout_ms = (int)luaL_optinteger(L, 3, -1);
    int res;

    if (s->handle == NULL) {
        return luaL_error(L, "attempt to wait on closed socket");
    }

    if (lua_type(L, 2) == LUA_TNUMBER) {
        events = (int)lua_tointeger(L, 2);
    } else if (lua_type(L, 2) == LUA_TSTRING) {
        const char *ev = lua_tostring(L, 2);
        events = 0;
        if (strchr(ev, 'r') != NULL) events |= SOLO_SOCK_POLL_READ;
        if (strchr(ev, 'w') != NULL) events |= SOLO_SOCK_POLL_WRITE;
        if (events == 0) events = SOLO_SOCK_POLL_READ;
    }

    res = solo_socket_wait(s->handle, events, timeout_ms);
    if (res < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "wait error");
        return 2;
    }
    if (res == 0) {
        lua_pushnil(L);
        lua_pushstring(L, "timeout");
        return 2;
    }

    if ((res & SOLO_SOCK_POLL_READ) && (res & SOLO_SOCK_POLL_WRITE)) {
        lua_pushstring(L, "rw");
    } else if (res & SOLO_SOCK_POLL_READ) {
        lua_pushstring(L, "r");
    } else if (res & SOLO_SOCK_POLL_WRITE) {
        lua_pushstring(L, "w");
    } else {
        lua_pushstring(L, "err");
    }
    return 1;
}

static int lsocket_get_fd(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    if (s->handle == NULL) {
        lua_pushinteger(L, -1);
    } else {
        lua_pushinteger(L, solo_socket_get_fd(s->handle));
    }
    return 1;
}

static int lsocket_type(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    if (s->handle == NULL) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, (solo_socket_get_type(s->handle) == SOLO_SOCK_UDP) ? "udp" : "tcp");
    return 1;
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

static int lsocket_tostring(lua_State *L)
{
    lsocket_t *s = (lsocket_t *)luaL_checkudata(L, 1, SOCKET_MT);
    char buf[128];
    if (s->handle == NULL) {
        lua_pushstring(L, "solo.socket (closed)");
    } else {
        solo_socket_type_t t = solo_socket_get_type(s->handle);
        solo_socket_mode_t m = solo_socket_get_mode(s->handle);
        int fd = solo_socket_get_fd(s->handle);
        snprintf(buf, sizeof(buf), "solo.socket (%s, fd: %d, %s)",
                 (t == SOLO_SOCK_UDP) ? "udp" : "tcp",
                 fd,
                 (m == SOLO_SOCK_NONBLOCKING) ? "nonblocking" : "blocking");
        lua_pushstring(L, buf);
    }
    return 1;
}

/* =============================================================================
 * Module Constructors
 * ============================================================================= */

static int lsocket_create(lua_State *L)
{
    solo_socket_type_t type = parse_type(L, 1);
    solo_socket_mode_t mode = parse_mode(L, 2, SOLO_SOCK_BLOCKING);
    char *err_msg = NULL;
    solo_socket_t *sock = solo_socket_create(type, mode, &err_msg);

    if (sock == NULL) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to create socket");
        }
        return 2;
    }

    push_socket_udata(L, sock);
    return 1;
}

static int lsocket_tcp(lua_State *L)
{
    solo_socket_mode_t mode = parse_mode(L, 1, SOLO_SOCK_BLOCKING);
    char *err_msg = NULL;
    solo_socket_t *sock = solo_socket_tcp(mode, &err_msg);

    if (sock == NULL) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to create TCP socket");
        }
        return 2;
    }

    push_socket_udata(L, sock);
    return 1;
}

static int lsocket_udp(lua_State *L)
{
    char *err_msg = NULL;
    solo_socket_t *sock;

    if (lua_gettop(L) >= 2 && lua_isstring(L, 1) && lua_isnumber(L, 2)) {
        const char *host = lua_tostring(L, 1);
        int port = (int)lua_tointeger(L, 2);
        solo_socket_mode_t mode = parse_mode(L, 3, SOLO_SOCK_BLOCKING);
        sock = solo_socket_udp_bind(host, port, mode, &err_msg);
    } else {
        solo_socket_mode_t mode = parse_mode(L, 1, SOLO_SOCK_BLOCKING);
        sock = solo_socket_udp(mode, &err_msg);
    }

    if (sock == NULL) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to create UDP socket");
        }
        return 2;
    }

    push_socket_udata(L, sock);
    return 1;
}

static int lsocket_udp_bind(lua_State *L)
{
    const char *host = luaL_optstring(L, 1, "0.0.0.0");
    int port = (int)luaL_optinteger(L, 2, 9100);
    solo_socket_mode_t mode = parse_mode(L, 3, SOLO_SOCK_BLOCKING);
    char *err_msg = NULL;
    solo_socket_t *sock = solo_socket_udp_bind(host, port, mode, &err_msg);

    if (sock == NULL) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to bind UDP socket");
        }
        return 2;
    }

    push_socket_udata(L, sock);
    return 1;
}

static int lsocket_connect(lua_State *L)
{
    const char *host = luaL_checkstring(L, 1);
    int port = (int)luaL_checkinteger(L, 2);
    solo_socket_mode_t mode = parse_mode(L, 3, SOLO_SOCK_BLOCKING);
    char *err_msg = NULL;
    solo_socket_t *sock = solo_socket_connect(host, port, mode, &err_msg);

    if (sock == NULL) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to connect");
        }
        return 2;
    }

    push_socket_udata(L, sock);
    return 1;
}

static int lsocket_bind(lua_State *L)
{
    const char *host = luaL_optstring(L, 1, "0.0.0.0");
    int port = (int)luaL_optinteger(L, 2, 9100);
    int backlog = (int)luaL_optinteger(L, 3, 128);
    solo_socket_mode_t mode = parse_mode(L, 4, SOLO_SOCK_BLOCKING);
    char *err_msg = NULL;
    solo_socket_t *sock = solo_socket_bind(host, port, backlog, mode, &err_msg);

    if (sock == NULL) {
        lua_pushnil(L);
        if (err_msg != NULL) {
            lua_pushstring(L, err_msg);
            free(err_msg);
        } else {
            lua_pushstring(L, "failed to bind socket");
        }
        return 2;
    }

    push_socket_udata(L, sock);
    return 1;
}

static int lsocket_select(lua_State *L)
{
    fd_set read_fds, write_fds;
    int max_fd = -1;
    int timeout_ms = -1;
    struct timeval tv;
    struct timeval *ptv = NULL;
    int num_readers = 0;
    int num_writers = 0;
    int i, ret;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        timeout_ms = (int)luaL_checkinteger(L, 3);
        if (timeout_ms >= 0) {
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            ptv = &tv;
        }
    }

    if (lua_istable(L, 1)) {
        num_readers = (int)lua_rawlen(L, 1);
        for (i = 1; i <= num_readers; i++) {
            lsocket_t *s;
            int fd;
            lua_rawgeti(L, 1, i);
            s = (lsocket_t *)luaL_testudata(L, -1, SOCKET_MT);
            if (s != NULL && s->handle != NULL) {
                fd = solo_socket_get_fd(s->handle);
                if (fd >= 0) {
                    FD_SET(fd, &read_fds);
                    if (fd > max_fd) max_fd = fd;
                }
            }
            lua_pop(L, 1);
        }
    }

    if (lua_istable(L, 2)) {
        num_writers = (int)lua_rawlen(L, 2);
        for (i = 1; i <= num_writers; i++) {
            lsocket_t *s;
            int fd;
            lua_rawgeti(L, 2, i);
            s = (lsocket_t *)luaL_testudata(L, -1, SOCKET_MT);
            if (s != NULL && s->handle != NULL) {
                fd = solo_socket_get_fd(s->handle);
                if (fd >= 0) {
                    FD_SET(fd, &write_fds);
                    if (fd > max_fd) max_fd = fd;
                }
            }
            lua_pop(L, 1);
        }
    }

    ret = select(max_fd + 1, &read_fds, &write_fds, NULL, ptv);
    if (ret < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "select failed");
        return 2;
    }

    lua_newtable(L);
    if (ret > 0 && lua_istable(L, 1)) {
        int out_idx = 1;
        for (i = 1; i <= num_readers; i++) {
            lsocket_t *s;
            int fd;
            lua_rawgeti(L, 1, i);
            s = (lsocket_t *)luaL_testudata(L, -1, SOCKET_MT);
            if (s != NULL && s->handle != NULL) {
                fd = solo_socket_get_fd(s->handle);
                if (fd >= 0 && FD_ISSET(fd, &read_fds)) {
                    lua_pushvalue(L, -1);
                    lua_rawseti(L, -3, out_idx++);
                }
            }
            lua_pop(L, 1);
        }
    }

    lua_newtable(L);
    if (ret > 0 && lua_istable(L, 2)) {
        int out_idx = 1;
        for (i = 1; i <= num_writers; i++) {
            lsocket_t *s;
            int fd;
            lua_rawgeti(L, 2, i);
            s = (lsocket_t *)luaL_testudata(L, -1, SOCKET_MT);
            if (s != NULL && s->handle != NULL) {
                fd = solo_socket_get_fd(s->handle);
                if (fd >= 0 && FD_ISSET(fd, &write_fds)) {
                    lua_pushvalue(L, -1);
                    lua_rawseti(L, -3, out_idx++);
                }
            }
            lua_pop(L, 1);
        }
    }

    return 2;
}

static const luaL_Reg socket_methods[] = {
    {"send",         lsocket_send        },
    {"write",        lsocket_send        },
    {"recv",         lsocket_recv        },
    {"read",         lsocket_recv        },
    {"sendto",       lsocket_sendto      },
    {"recvfrom",     lsocket_recvfrom    },
    {"bind",         lsocket_bind_sock   },
    {"connect",      lsocket_connect_sock},
    {"accept",       lsocket_accept      },
    {"set_mode",     lsocket_set_mode    },
    {"set_blocking", lsocket_set_blocking},
    {"get_mode",     lsocket_get_mode    },
    {"is_blocking",  lsocket_is_blocking },
    {"set_timeout",  lsocket_set_timeout },
    {"wait",         lsocket_wait        },
    {"get_fd",       lsocket_get_fd      },
    {"type",         lsocket_type        },
    {"close",        lsocket_close       },
    {"__close",      lsocket_close       },
    {"__gc",         lsocket_close       },
    {"__tostring",   lsocket_tostring    },
    {NULL,           NULL                }
};

static const luaL_Reg socket_funcs[] = {
    {"create",   lsocket_create  },
    {"tcp",      lsocket_tcp     },
    {"udp",      lsocket_udp     },
    {"udp_bind", lsocket_udp_bind},
    {"connect",  lsocket_connect },
    {"bind",     lsocket_bind    },
    {"select",   lsocket_select  },
    {NULL,       NULL            }
};

static void register_enums(lua_State *L)
{
    /* solo.socket.type */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SOCK_TCP); lua_setfield(L, -2, "TCP");
    lua_pushinteger(L, SOLO_SOCK_UDP); lua_setfield(L, -2, "UDP");
    lua_setfield(L, -2, "type");

    /* solo.socket.mode */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SOCK_BLOCKING);    lua_setfield(L, -2, "BLOCKING");
    lua_pushinteger(L, SOLO_SOCK_NONBLOCKING); lua_setfield(L, -2, "NONBLOCKING");
    lua_setfield(L, -2, "mode");

    /* solo.socket.status */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SOCK_OK);     lua_setfield(L, -2, "OK");
    lua_pushinteger(L, SOLO_SOCK_ERROR);  lua_setfield(L, -2, "ERROR");
    lua_pushinteger(L, SOLO_SOCK_AGAIN);  lua_setfield(L, -2, "AGAIN");
    lua_pushinteger(L, SOLO_SOCK_CLOSED); lua_setfield(L, -2, "CLOSED");
    lua_setfield(L, -2, "status");

    /* solo.socket.event */
    lua_newtable(L);
    lua_pushinteger(L, SOLO_SOCK_POLL_READ);  lua_setfield(L, -2, "READ");
    lua_pushinteger(L, SOLO_SOCK_POLL_WRITE); lua_setfield(L, -2, "WRITE");
    lua_pushinteger(L, SOLO_SOCK_POLL_ERR);   lua_setfield(L, -2, "ERR");
    lua_setfield(L, -2, "event");

    /* Direct enum constants */
    lua_pushinteger(L, SOLO_SOCK_TCP);         lua_setfield(L, -2, "TCP");
    lua_pushinteger(L, SOLO_SOCK_UDP);         lua_setfield(L, -2, "UDP");
    lua_pushinteger(L, SOLO_SOCK_BLOCKING);    lua_setfield(L, -2, "BLOCKING");
    lua_pushinteger(L, SOLO_SOCK_NONBLOCKING); lua_setfield(L, -2, "NONBLOCKING");
}

int luaopen_solo_socket(lua_State *L)
{
    if (luaL_newmetatable(L, SOCKET_MT)) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, socket_methods, 0);
    }
    lua_pop(L, 1);

    luaL_newlib(L, socket_funcs);
    register_enums(L);
    return 1;
}
