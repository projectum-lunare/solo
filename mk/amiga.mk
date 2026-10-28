# mk/amiga.mk - AmigaOS m68k Target (m68k-amigaos-gcc)

AMIGA_SDK ?= $(HOME)/.local/opt/amigaos-sdk

AMIGA_BUILD_DIR ?= build/amiga

AMIGA_CC     ?= $(AMIGA_SDK)/bin/m68k-amigaos-gcc
AMIGA_AR     ?= $(AMIGA_SDK)/bin/m68k-amigaos-ar
AMIGA_RANLIB ?= $(AMIGA_SDK)/bin/m68k-amigaos-ranlib

AMIGA_CFLAGS ?= -Wall -Wextra -O2 -std=gnu89 -m68000 -msoft-float -noixemul -Iinclude

AMIGA_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_amiga.c \
    src/serial/serial_amiga.c \
    src/socket/socket_amiga.c \
    src/sandbox/sandbox_stub.c

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
