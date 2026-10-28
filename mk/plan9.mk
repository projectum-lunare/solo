# mk/plan9.mk - Plan 9 Target (APE / GCC Plan 9)

PLAN9_BUILD_DIR ?= build/plan9

PLAN9_CC     ?= gcc
PLAN9_AR     ?= ar
PLAN9_RANLIB ?= ranlib

ALL_MODULES ?= platform filesystem parallel serial socket sandbox
MODULES     ?= $(ALL_MODULES)
SOLO_MODULE_DEFS ?= \
    $(if $(filter platform,$(MODULES)),,-DSOLO_NO_PLATFORM) \
    $(if $(filter filesystem,$(MODULES)),,-DSOLO_NO_FILESYSTEM) \
    $(if $(filter parallel,$(MODULES)),,-DSOLO_NO_PARALLEL) \
    $(if $(filter serial,$(MODULES)),,-DSOLO_NO_SERIAL) \
    $(if $(filter socket,$(MODULES)),,-DSOLO_NO_SOCKET) \
    $(if $(filter sandbox,$(MODULES)),,-DSOLO_NO_SANDBOX)

PLAN9_CFLAGS ?= -Wall -Wextra -O2 -std=c89 -D_PLAN9_SOURCE -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE $(SOLO_MODULE_DEFS) -Iinclude

PLAN9_SRCS = \
    src/solo.c \
    $(if $(filter platform,$(MODULES)),src/platform/platform.c) \
    $(if $(filter filesystem,$(MODULES)),src/filesystem/filesystem.c) \
    $(if $(filter parallel,$(MODULES)),src/parallel/parallel_plan9.c) \
    $(if $(filter serial,$(MODULES)),src/serial/serial_plan9.c) \
    $(if $(filter socket,$(MODULES)),src/socket/socket_plan9.c) \
    $(if $(filter sandbox,$(MODULES)),src/sandbox/sandbox_stub.c)

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
