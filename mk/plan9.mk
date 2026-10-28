# mk/plan9.mk - Plan 9 Target (APE / GCC Plan 9)

PLAN9_BUILD_DIR ?= build/plan9

PLAN9_CC     ?= gcc
PLAN9_AR     ?= ar
PLAN9_RANLIB ?= ranlib

PLAN9_CFLAGS ?= -Wall -Wextra -O2 -std=c89 -D_PLAN9_SOURCE -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -Iinclude

PLAN9_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_plan9.c \
    src/serial/serial_plan9.c \
    src/socket/socket_plan9.c \
    src/sandbox/sandbox_stub.c

PLAN9_OBJS = $(patsubst src/%.c,$(PLAN9_BUILD_DIR)/%.o,$(PLAN9_SRCS))

.PHONY: plan9
plan9: $(PLAN9_BUILD_DIR)/libsolo.a

$(PLAN9_BUILD_DIR)/libsolo.a: $(PLAN9_OBJS)
	@mkdir -p $(dir $@)
	$(PLAN9_AR) rcs $@ $^
	$(PLAN9_RANLIB) $@
	@echo "==> Built Plan 9 static library: $@"

$(PLAN9_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(PLAN9_CC) $(PLAN9_CFLAGS) -c $< -o $@
