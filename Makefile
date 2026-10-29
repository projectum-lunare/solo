# solo - Physical I/O, System Foundations & Hardware Abstractions
# Projectum Lunare <https://github.com/projectum-lunare/solo>
# Copyright (C) 2026 Gustavo Veiga <gustavo.veiga@outlook.com.br>
# Mozilla Public License Version 2.0

CC     ?= gcc
AR     ?= ar
RANLIB ?= ranlib
LUA    ?= lua

CFLAGS ?= -Wall -Wextra -O2 -std=c89 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE

# Lua 5.4+ include directory resolution (searches custom LUA_INCDIR, parent/sibling repos, system Lua 5.5/5.4)
LUA_SEARCH_DIRS = \
    $(LUA_INCDIR) \
    ../vendor/lua \
    ../lua \
    ../../vendor/lua \
    ../dotstar/vendor/lua \
    /usr/include/lua5.5 \
    /usr/include/lua55 \
    /usr/include/lua-5.5 \
    /usr/local/include/lua5.5 \
    /usr/include/lua5.4 \
    /usr/include/lua54 \
    /usr/include/lua-5.4 \
    /usr/local/include/lua5.4 \
    /usr/include/lua

RESOLVED_LUA_INCDIR := $(firstword $(foreach d,$(LUA_SEARCH_DIRS),$(if $(wildcard $(d)/lua.h),$(d))))
INCLUDES = -Iinclude -Ivendor $(if $(RESOLVED_LUA_INCDIR),-I$(RESOLVED_LUA_INCDIR))

SO_EXT  ?= so
LIBFLAG ?= -shared
LDLIBS  ?=

.DEFAULT_GOAL := all
.PHONY: all static lua modules test install clean check-all all-platforms

# Modular subsystem selection (default: all)
ALL_MODULES = platform filesystem parallel serial socket sandbox
MODULES    ?= $(ALL_MODULES)

# Feature flags for CFLAGS based on excluded modules
ifeq ($(filter platform,$(MODULES)),)
    CFLAGS += -DSOLO_NO_PLATFORM
endif
ifeq ($(filter filesystem,$(MODULES)),)
    CFLAGS += -DSOLO_NO_FILESYSTEM
endif
ifeq ($(filter parallel,$(MODULES)),)
    CFLAGS += -DSOLO_NO_PARALLEL
endif
ifeq ($(filter serial,$(MODULES)),)
    CFLAGS += -DSOLO_NO_SERIAL
endif
ifeq ($(filter socket,$(MODULES)),)
    CFLAGS += -DSOLO_NO_SOCKET
endif
ifeq ($(filter sandbox,$(MODULES)),)
    CFLAGS += -DSOLO_NO_SANDBOX
endif

# Modular platform targets
-include mk/*.mk

# Host targets
LIB_STATIC = $(POSIX_BUILD_DIR)/libsolo.a
LUA_MOD    = solo.$(SO_EXT)

LUA_CORE_SRCS = \
    $(if $(filter platform,$(MODULES)),src/platform/platform_lua.c) \
    $(if $(filter filesystem,$(MODULES)),src/filesystem/filesystem_lua.c) \
    $(if $(filter parallel,$(MODULES)),src/parallel/parallel_lua.c) \
    $(if $(filter serial,$(MODULES)),src/serial/serial_lua.c) \
    $(if $(filter socket,$(MODULES)),src/socket/socket_lua.c) \
    $(if $(filter sandbox,$(MODULES)),src/sandbox/sandbox_lua.c) \
    src/solo_lua.c

PIC_OBJS = $(patsubst src/%.c,build/pic/%.o,$(POSIX_SRCS) $(LUA_CORE_SRCS))

# Standalone Lua submodules
MOD_DIR = build/lua/solo
LUA_SUBMODULES = \
    $(if $(filter platform,$(MODULES)),$(MOD_DIR)/platform.$(SO_EXT)) \
    $(if $(filter filesystem,$(MODULES)),$(MOD_DIR)/filesystem.$(SO_EXT) $(MOD_DIR)/fs.$(SO_EXT)) \
    $(if $(filter parallel,$(MODULES)),$(MOD_DIR)/parallel.$(SO_EXT)) \
    $(if $(filter serial,$(MODULES)),$(MOD_DIR)/serial.$(SO_EXT)) \
    $(if $(filter socket,$(MODULES)),$(MOD_DIR)/socket.$(SO_EXT)) \
    $(if $(filter sandbox,$(MODULES)),$(MOD_DIR)/sandbox.$(SO_EXT))

all: static lua modules
static: $(LIB_STATIC)
lua: $(LUA_MOD)
modules: $(LUA_SUBMODULES)

# Umbrella Lua module
$(LUA_MOD): $(PIC_OBJS)
	$(CC) $(LIBFLAG) $(CFLAGS) -o $@ $^ $(LDLIBS)

# Standalone Lua submodules
$(MOD_DIR)/platform.$(SO_EXT): build/pic/platform/platform_lua.o build/pic/platform/platform.o
	@mkdir -p $(dir $@)
	$(CC) $(LIBFLAG) $(CFLAGS) -o $@ $^

$(MOD_DIR)/filesystem.$(SO_EXT): build/pic/filesystem/filesystem_lua.o build/pic/filesystem/filesystem.o build/pic/platform/platform.o
	@mkdir -p $(dir $@)
	$(CC) $(LIBFLAG) $(CFLAGS) -o $@ $^

$(MOD_DIR)/fs.$(SO_EXT): $(MOD_DIR)/filesystem.$(SO_EXT)
	@mkdir -p $(dir $@)
	@cp $< $@

$(MOD_DIR)/parallel.$(SO_EXT): build/pic/parallel/parallel_lua.o build/pic/parallel/parallel_posix.o build/pic/platform/platform.o
	@mkdir -p $(dir $@)
	$(CC) $(LIBFLAG) $(CFLAGS) -o $@ $^

$(MOD_DIR)/serial.$(SO_EXT): build/pic/serial/serial_lua.o build/pic/serial/serial_posix.o build/pic/platform/platform.o
	@mkdir -p $(dir $@)
	$(CC) $(LIBFLAG) $(CFLAGS) -o $@ $^

$(MOD_DIR)/socket.$(SO_EXT): build/pic/socket/socket_lua.o build/pic/socket/socket_posix.o build/pic/platform/platform.o
	@mkdir -p $(dir $@)
	$(CC) $(LIBFLAG) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(MOD_DIR)/sandbox.$(SO_EXT): build/pic/sandbox/sandbox_lua.o $(patsubst src/%.c,build/pic/%.o,$(POSIX_SANDBOX_BACKEND)) build/pic/platform/platform.o
	@mkdir -p $(dir $@)
	$(CC) $(LIBFLAG) $(CFLAGS) -o $@ $^

build/pic/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC $(INCLUDES) -c $< -o $@

test: $(LUA_MOD) $(LIB_STATIC) $(LUA_SUBMODULES)
	@echo "==> Building C unit tests..."
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_c_api.c $(LIB_STATIC) -o tests/test_c_api $(LDLIBS)
	@echo "==> Running C unit tests..."
	./tests/test_c_api
	@echo "==> Running Lua test suite (Runner & Harness)..."
	$(LUA) tests/runner.lua

install: $(LUA_MOD) $(LUA_SUBMODULES)
	@mkdir -p $(DESTDIR)$(INST_LIBDIR)/solo
	cp $(LUA_MOD) $(DESTDIR)$(INST_LIBDIR)/
	cp $(LUA_SUBMODULES) $(DESTDIR)$(INST_LIBDIR)/solo/

clean:
	rm -f $(LUA_MOD) libsolo.* tests/test_c_api
	rm -rf build/

check-all: posix dos os2 winnt wince amiga atarist kolibri plan9 stub
	@echo "=========================================================="
	@echo " All 10 platforms built successfully!"
	@echo "=========================================================="

all-platforms: check-all
