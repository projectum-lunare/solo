# mk/posix.mk - POSIX Target (Linux, BSD, macOS)

POSIX_BUILD_DIR ?= build/posix

POSIX_CC     ?= gcc
POSIX_AR     ?= ar
POSIX_RANLIB ?= ranlib

POSIX_CFLAGS ?= -Wall -Wextra -O2 -std=c89 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -Iinclude

POSIX_UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
ifeq ($(POSIX_UNAME_S),Linux)
    POSIX_SANDBOX_BACKEND = src/sandbox/sandbox_linux.c
else ifeq ($(POSIX_UNAME_S),OpenBSD)
    POSIX_SANDBOX_BACKEND = src/sandbox/sandbox_openbsd.c
else ifeq ($(POSIX_UNAME_S),FreeBSD)
    POSIX_SANDBOX_BACKEND = src/sandbox/sandbox_freebsd.c
else
    POSIX_SANDBOX_BACKEND = src/sandbox/sandbox_stub.c
endif

POSIX_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_posix.c \
    src/serial/serial_posix.c \
    src/socket/socket_posix.c \
    $(POSIX_SANDBOX_BACKEND)

POSIX_OBJS = $(patsubst src/%.c,$(POSIX_BUILD_DIR)/%.o,$(POSIX_SRCS))

.PHONY: posix
posix: $(POSIX_BUILD_DIR)/libsolo.a

$(POSIX_BUILD_DIR)/libsolo.a: $(POSIX_OBJS)
	@mkdir -p $(dir $@)
	$(POSIX_AR) rcs $@ $^
	$(POSIX_RANLIB) $@
	@echo "==> Built POSIX static library: $@"

$(POSIX_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(POSIX_CC) $(POSIX_CFLAGS) -c $< -o $@
