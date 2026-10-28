# solo - Physical I/O, System Foundations & Hardware Abstractions
# Projectum Lunare <https://github.com/projectum-lunare/solo>
# Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
# Mozilla Public License Version 2.0

CC     ?= gcc
AR     ?= ar
RANLIB ?= ranlib
LUA    ?= lua

CFLAGS ?= -Wall -Wextra -O2 -std=c89 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
INCLUDES = -Iinclude $(if $(LUA_INCDIR),-I$(LUA_INCDIR),-Ivendor/lua)

SO_EXT  ?= so
LIBFLAG ?= -shared
LDLIBS  ?=

.DEFAULT_GOAL := all
.PHONY: all static lua test install clean check-all all-platforms

# Modular platform targets
-include mk/*.mk

# Host targets
LIB_STATIC = $(POSIX_BUILD_DIR)/libsolo.a
LUA_MOD    = solo.$(SO_EXT)

LUA_SRCS = $(wildcard src/*/*_lua.c) src/solo_lua.c
PIC_OBJS = $(patsubst src/%.c,build/pic/%.o,$(POSIX_SRCS) $(LUA_SRCS))

all: static lua
static: $(LIB_STATIC)
lua: $(LUA_MOD)

$(LUA_MOD): $(PIC_OBJS)
	$(CC) $(LIBFLAG) $(CFLAGS) -o $@ $^ $(LDLIBS)

build/pic/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC $(INCLUDES) -c $< -o $@

test: $(LUA_MOD) $(LIB_STATIC)
	@echo "==> Building C unit tests..."
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_c_api.c $(LIB_STATIC) -o tests/test_c_api $(LDLIBS)
	@echo "==> Running C unit tests..."
	./tests/test_c_api
	@echo "==> Running Lua test suite..."
	$(LUA) tests/test_solo.lua

install: $(LUA_MOD)
	@mkdir -p $(DESTDIR)$(INST_LIBDIR)
	cp $(LUA_MOD) $(DESTDIR)$(INST_LIBDIR)/

clean:
	rm -f $(LUA_MOD) libsolo.* tests/test_c_api
	rm -rf build/

check-all: posix dos os2 winnt wince amiga atarist kolibri plan9 stub
	@echo "=========================================================="
	@echo " All 10 platforms built successfully!"
	@echo "=========================================================="

all-platforms: check-all
