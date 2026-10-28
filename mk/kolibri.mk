# mk/kolibri.mk - KolibriOS 32-bit Target (kos32-gcc)

KOLIBRI_SDK ?= $(HOME)/.local/opt/kolibrios-sdk

KOLIBRI_BUILD_DIR ?= build/kolibri

KOLIBRI_CC ?= $(KOLIBRI_SDK)/bin/kos32-gcc
KOLIBRI_AR ?= $(KOLIBRI_SDK)/bin/kos32-ar

KOLIBRI_CFLAGS ?= -Wall -Wextra -O2 -std=gnu89 -nostdlib -U_WIN32 -D__KOLIBRIOS__ -Iinclude -I$(KOLIBRI_SDK)/include

KOLIBRI_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_kolibri.c \
    src/serial/serial_kolibri.c \
    src/socket/socket_kolibri.c \
    src/sandbox/sandbox_stub.c

KOLIBRI_OBJS = $(patsubst src/%.c,$(KOLIBRI_BUILD_DIR)/%.o,$(KOLIBRI_SRCS))

.PHONY: kolibri kolibrios
kolibri: $(KOLIBRI_BUILD_DIR)/libsolo.a
kolibrios: kolibri

$(KOLIBRI_BUILD_DIR)/libsolo.a: $(KOLIBRI_OBJS)
	@mkdir -p $(dir $@)
	$(KOLIBRI_AR) rcs $@ $^
	@echo "==> Built KolibriOS static library: $@"

$(KOLIBRI_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(KOLIBRI_CC) $(KOLIBRI_CFLAGS) -c $< -o $@
