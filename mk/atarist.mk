# mk/atarist.mk - Atari ST m68k Target (m68k-atari-mint-gcc)

ATARIST_SDK ?= $(HOME)/.local/opt/atarist-sdk

ATARIST_BUILD_DIR ?= build/atarist

ATARIST_CC     ?= $(ATARIST_SDK)/bin/m68k-atari-mint-gcc
ATARIST_AR     ?= $(ATARIST_SDK)/bin/m68k-atari-mint-ar
ATARIST_RANLIB ?= $(ATARIST_SDK)/bin/m68k-atari-mint-ranlib

ALL_MODULES ?= platform filesystem parallel serial socket sandbox
MODULES     ?= $(ALL_MODULES)
SOLO_MODULE_DEFS ?= \
    $(if $(filter platform,$(MODULES)),,-DSOLO_NO_PLATFORM) \
    $(if $(filter filesystem,$(MODULES)),,-DSOLO_NO_FILESYSTEM) \
    $(if $(filter parallel,$(MODULES)),,-DSOLO_NO_PARALLEL) \
    $(if $(filter serial,$(MODULES)),,-DSOLO_NO_SERIAL) \
    $(if $(filter socket,$(MODULES)),,-DSOLO_NO_SOCKET) \
    $(if $(filter sandbox,$(MODULES)),,-DSOLO_NO_SANDBOX)

ATARIST_CFLAGS ?= -Wall -Wextra -O2 -std=gnu89 -m68000 -msoft-float $(SOLO_MODULE_DEFS) -Iinclude

ATARIST_SRCS = \
    src/solo.c \
    $(if $(filter platform,$(MODULES)),src/platform/platform.c) \
    $(if $(filter filesystem,$(MODULES)),src/filesystem/filesystem.c) \
    $(if $(filter parallel,$(MODULES)),src/parallel/parallel_atarist.c) \
    $(if $(filter serial,$(MODULES)),src/serial/serial_atarist.c) \
    $(if $(filter socket,$(MODULES)),src/socket/socket_atarist.c) \
    $(if $(filter sandbox,$(MODULES)),src/sandbox/sandbox_stub.c)

ATARIST_OBJS = $(patsubst src/%.c,$(ATARIST_BUILD_DIR)/%.o,$(ATARIST_SRCS))

.PHONY: atarist atari
atarist: $(ATARIST_BUILD_DIR)/libsolo.a
atari: atarist

$(ATARIST_BUILD_DIR)/libsolo.a: $(ATARIST_OBJS)
	@mkdir -p $(dir $@)
	$(ATARIST_AR) rcs $@ $^
	$(ATARIST_RANLIB) $@
	@echo "==> Built Atari ST static library: $@"

$(ATARIST_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(ATARIST_CC) $(ATARIST_CFLAGS) -c $< -o $@
