# mk/amiga.mk - AmigaOS m68k Target (m68k-amigaos-gcc)

AMIGA_SDK ?= $(HOME)/.local/opt/amigaos-sdk

AMIGA_BUILD_DIR ?= build/amiga

AMIGA_CC     ?= $(AMIGA_SDK)/bin/m68k-amigaos-gcc
AMIGA_AR     ?= $(AMIGA_SDK)/bin/m68k-amigaos-ar
AMIGA_RANLIB ?= $(AMIGA_SDK)/bin/m68k-amigaos-ranlib

ALL_MODULES ?= platform filesystem parallel serial socket sandbox
MODULES     ?= $(ALL_MODULES)
SOLO_MODULE_DEFS ?= \
    $(if $(filter platform,$(MODULES)),,-DSOLO_NO_PLATFORM) \
    $(if $(filter filesystem,$(MODULES)),,-DSOLO_NO_FILESYSTEM) \
    $(if $(filter parallel,$(MODULES)),,-DSOLO_NO_PARALLEL) \
    $(if $(filter serial,$(MODULES)),,-DSOLO_NO_SERIAL) \
    $(if $(filter socket,$(MODULES)),,-DSOLO_NO_SOCKET) \
    $(if $(filter sandbox,$(MODULES)),,-DSOLO_NO_SANDBOX)

AMIGA_CFLAGS ?= -Wall -Wextra -O2 -std=gnu89 -m68000 -msoft-float -noixemul $(SOLO_MODULE_DEFS) -Iinclude

AMIGA_SRCS = \
    src/solo.c \
    $(if $(filter platform,$(MODULES)),src/platform/platform.c) \
    $(if $(filter filesystem,$(MODULES)),src/filesystem/filesystem.c) \
    $(if $(filter parallel,$(MODULES)),src/parallel/parallel_amiga.c) \
    $(if $(filter serial,$(MODULES)),src/serial/serial_amiga.c) \
    $(if $(filter socket,$(MODULES)),src/socket/socket_amiga.c) \
    $(if $(filter sandbox,$(MODULES)),src/sandbox/sandbox_stub.c)

AMIGA_OBJS = $(patsubst src/%.c,$(AMIGA_BUILD_DIR)/%.o,$(AMIGA_SRCS))

.PHONY: amiga amigaos
amiga: $(AMIGA_BUILD_DIR)/libsolo.a
amigaos: amiga

$(AMIGA_BUILD_DIR)/libsolo.a: $(AMIGA_OBJS)
	@mkdir -p $(dir $@)
	$(AMIGA_AR) rcs $@ $^
	$(AMIGA_RANLIB) $@
	@echo "==> Built AmigaOS static library: $@"

$(AMIGA_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(AMIGA_CC) $(AMIGA_CFLAGS) -c $< -o $@
