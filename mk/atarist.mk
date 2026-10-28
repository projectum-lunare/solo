# mk/atarist.mk - Atari ST / FreeMiNT Target (m68k-atari-mint-gcc)

ATARI_SDK ?= $(HOME)/.local/opt/atarist-sdk

ATARI_BUILD_DIR ?= build/atarist

ATARI_CC     ?= $(ATARI_SDK)/bin/m68k-atari-mint-gcc
ATARI_AR     ?= $(ATARI_SDK)/bin/m68k-atari-mint-ar
ATARI_RANLIB ?= $(ATARI_SDK)/bin/m68k-atari-mint-ranlib

ATARI_CFLAGS ?= -Wall -Wextra -O2 -std=gnu89 -m68000 -msoft-float -Iinclude

ATARI_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_atarist.c \
    src/serial/serial_atarist.c \
    src/socket/socket_atarist.c \
    src/sandbox/sandbox_stub.c

ATARI_OBJS = $(patsubst src/%.c,$(ATARI_BUILD_DIR)/%.o,$(ATARI_SRCS))

.PHONY: atarist atari
atarist: $(ATARI_BUILD_DIR)/libsolo.a
atari: atarist

$(ATARI_BUILD_DIR)/libsolo.a: $(ATARI_OBJS)
	@mkdir -p $(dir $@)
	$(ATARI_AR) rcs $@ $^
	$(ATARI_RANLIB) $@
	@echo "==> Built Atari ST static library: $@"

$(ATARI_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(ATARI_CC) $(ATARI_CFLAGS) -c $< -o $@
