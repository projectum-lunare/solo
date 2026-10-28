# mk/stub.mk - Generic / Stub Target (C89 Fallback)

STUB_BUILD_DIR ?= build/stub

STUB_CC     ?= gcc
STUB_AR     ?= ar
STUB_RANLIB ?= ranlib

STUB_CFLAGS ?= -Wall -Wextra -O2 -std=c89 -Iinclude

STUB_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_stub.c \
    src/serial/serial_stub.c \
    src/socket/socket_stub.c \
    src/sandbox/sandbox_stub.c

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
