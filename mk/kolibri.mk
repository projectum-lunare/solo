# mk/kolibri.mk - KolibriOS x86 Target (kos32-gcc)

KOLIBRI_SDK ?= $(HOME)/.local/opt/kolibrios-sdk

KOLIBRI_BUILD_DIR ?= build/kolibri

KOLIBRI_CC ?= $(KOLIBRI_SDK)/bin/kos32-gcc
KOLIBRI_AR ?= $(KOLIBRI_SDK)/bin/kos32-ar

ALL_MODULES ?= platform filesystem parallel serial socket sandbox
MODULES     ?= $(ALL_MODULES)
SOLO_MODULE_DEFS ?= \
    $(if $(filter platform,$(MODULES)),,-DSOLO_NO_PLATFORM) \
    $(if $(filter filesystem,$(MODULES)),,-DSOLO_NO_FILESYSTEM) \
    $(if $(filter parallel,$(MODULES)),,-DSOLO_NO_PARALLEL) \
    $(if $(filter serial,$(MODULES)),,-DSOLO_NO_SERIAL) \
    $(if $(filter socket,$(MODULES)),,-DSOLO_NO_SOCKET) \
    $(if $(filter sandbox,$(MODULES)),,-DSOLO_NO_SANDBOX)

KOLIBRI_CFLAGS ?= -Wall -Wextra -O2 -std=gnu89 -nostdlib -U_WIN32 -D__KOLIBRIOS__ $(SOLO_MODULE_DEFS) -Iinclude -I$(KOLIBRI_SDK)/include

KOLIBRI_SRCS = \
    src/solo.c \
    $(if $(filter platform,$(MODULES)),src/platform/platform.c) \
    $(if $(filter filesystem,$(MODULES)),src/filesystem/filesystem.c) \
    $(if $(filter parallel,$(MODULES)),src/parallel/parallel_kolibri.c) \
    $(if $(filter serial,$(MODULES)),src/serial/serial_kolibri.c) \
    $(if $(filter socket,$(MODULES)),src/socket/socket_kolibri.c) \
    $(if $(filter sandbox,$(MODULES)),src/sandbox/sandbox_stub.c)

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
