# solo - Physical I/O, System Foundations & Hardware Abstractions
# Projectum Lunare <https://github.com/projectum-lunare/solo>
# Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
# Mozilla Public License Version 2.0

CC ?= gcc
AR ?= ar
RANLIB ?= ranlib
LUA ?= lua

CFLAGS ?= -Wall -Wextra -O2 -std=c99
INCLUDES = -Iinclude -Ivendor/lua

# Detect OS
UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)

ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
    PLATFORM = windows_nt
    SO_EXT = dll
    LDFLAGS_SHARED = -shared
    LDLIBS = -lws2_32
else ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
    PLATFORM = windows_nt
    SO_EXT = dll
    LDFLAGS_SHARED = -shared
    LDLIBS = -lws2_32
else ifeq ($(UNAME_S),Darwin)
    PLATFORM = posix
    SO_EXT = dylib
    LDFLAGS_SHARED = -dynamiclib -undefined dynamic_lookup
    LDLIBS =
else
    PLATFORM = posix
    SO_EXT = so
    LDFLAGS_SHARED = -shared
    LDLIBS =
endif

# Sandbox backend selection
ifeq ($(UNAME_S),Linux)
    SANDBOX_BACKEND = src/sandbox/sandbox_linux.c
else ifeq ($(UNAME_S),OpenBSD)
    SANDBOX_BACKEND = src/sandbox/sandbox_openbsd.c
else ifeq ($(UNAME_S),FreeBSD)
    SANDBOX_BACKEND = src/sandbox/sandbox_freebsd.c
else
    SANDBOX_BACKEND = src/sandbox/sandbox_stub.c
endif

# C Core Sources
CORE_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_$(PLATFORM).c \
    src/serial/serial_$(PLATFORM).c \
    src/socket/socket_$(PLATFORM).c \
    $(SANDBOX_BACKEND)

# Lua Binding Sources
LUA_SRCS = \
    src/platform/platform_lua.c \
    src/filesystem/filesystem_lua.c \
    src/parallel/parallel_lua.c \
    src/serial/serial_lua.c \
    src/socket/socket_lua.c \
    src/sandbox/sandbox_lua.c \
    src/solo_lua.c

CORE_OBJS = $(CORE_SRCS:.c=.o)
CORE_PIC_OBJS = $(CORE_SRCS:.c=.pic.o)
LUA_PIC_OBJS = $(LUA_SRCS:.c=.pic.o)

LIB_STATIC = libsolo.a
LIB_SHARED = libsolo.$(SO_EXT)
LUA_MOD = solo.$(SO_EXT)

.PHONY: all static shared lua test clean install

all: static shared lua

static: $(LIB_STATIC)
shared: $(LIB_SHARED)
lua: $(LUA_MOD)

# Static library
$(LIB_STATIC): $(CORE_OBJS)
	$(AR) rcs $@ $^
	$(RANLIB) $@

# Shared C library
$(LIB_SHARED): $(CORE_PIC_OBJS)
	$(CC) $(LDFLAGS_SHARED) $(CFLAGS) -o $@ $^ $(LDLIBS)

# Lua C module (solo.so)
$(LUA_MOD): $(CORE_PIC_OBJS) $(LUA_PIC_OBJS)
	$(CC) $(LDFLAGS_SHARED) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

%.pic.o: %.c
	$(CC) $(CFLAGS) -fPIC $(INCLUDES) -c $< -o $@

test: $(LUA_MOD) $(LIB_STATIC)
	@echo "==> Building C unit tests..."
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_c_api.c $(LIB_STATIC) -o tests/test_c_api $(LDLIBS)
	@echo "==> Running C unit tests..."
	./tests/test_c_api
	@echo "==> Running Lua test suite..."
	$(LUA) tests/test_solo.lua

clean:
	rm -f $(CORE_OBJS) $(CORE_PIC_OBJS) $(LUA_PIC_OBJS)
	rm -f $(LIB_STATIC) $(LIB_SHARED) $(LUA_MOD)
	rm -f tests/test_c_api /tmp/solo.so
