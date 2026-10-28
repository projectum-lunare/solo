# mk/stub.mk - Generic / Stub Target (C89 Fallback)

STUB_BUILD_DIR ?= build/stub

STUB_CC     ?= gcc
STUB_AR     ?= ar
STUB_RANLIB ?= ranlib

ALL_MODULES ?= platform filesystem parallel serial socket sandbox
MODULES     ?= $(ALL_MODULES)
SOLO_MODULE_DEFS ?= \
    $(if $(filter platform,$(MODULES)),,-DSOLO_NO_PLATFORM) \
    $(if $(filter filesystem,$(MODULES)),,-DSOLO_NO_FILESYSTEM) \
    $(if $(filter parallel,$(MODULES)),,-DSOLO_NO_PARALLEL) \
    $(if $(filter serial,$(MODULES)),,-DSOLO_NO_SERIAL) \
    $(if $(filter socket,$(MODULES)),,-DSOLO_NO_SOCKET) \
    $(if $(filter sandbox,$(MODULES)),,-DSOLO_NO_SANDBOX)

STUB_CFLAGS ?= -Wall -Wextra -O2 -std=c89 $(SOLO_MODULE_DEFS) -Iinclude

STUB_SRCS = \
    src/solo.c \
    $(if $(filter platform,$(MODULES)),src/platform/platform.c) \
    $(if $(filter filesystem,$(MODULES)),src/filesystem/filesystem.c) \
    $(if $(filter parallel,$(MODULES)),src/parallel/parallel_stub.c) \
    $(if $(filter serial,$(MODULES)),src/serial/serial_stub.c) \
    $(if $(filter socket,$(MODULES)),src/socket/socket_stub.c) \
    $(if $(filter sandbox,$(MODULES)),src/sandbox/sandbox_stub.c)

STUB_OBJS = $(patsubst src/%.c,$(STUB_BUILD_DIR)/%.o,$(STUB_SRCS))

.PHONY: stub
stub: $(STUB_BUILD_DIR)/libsolo.a

$(STUB_BUILD_DIR)/libsolo.a: $(STUB_OBJS)
	@mkdir -p $(dir $@)
	$(STUB_AR) rcs $@ $^
	$(STUB_RANLIB) $@
	@echo "==> Built Stub static library: $@"

$(STUB_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(STUB_CC) $(STUB_CFLAGS) -c $< -o $@
