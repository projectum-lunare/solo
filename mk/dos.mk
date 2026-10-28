# mk/dos.mk - MS-DOS Target (Open Watcom + WATT-32)

WATCOM_DIR ?= $(HOME)/.local/opt/watcom
WATT32_DIR ?= $(HOME)/.local/opt/watt32

DOS_BUILD_DIR ?= build/dos

DOS_WCC  ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH EDPATH=$(WATCOM_DIR)/eddat INCLUDE=$(WATCOM_DIR)/h:$(WATCOM_DIR)/h/dos $(WATCOM_DIR)/binl64/wcc386
DOS_WLIB ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH $(WATCOM_DIR)/binl64/wlib

ALL_MODULES ?= platform filesystem parallel serial socket sandbox
MODULES     ?= $(ALL_MODULES)
SOLO_MODULE_DEFS ?= \
    $(if $(filter platform,$(MODULES)),,-DSOLO_NO_PLATFORM) \
    $(if $(filter filesystem,$(MODULES)),,-DSOLO_NO_FILESYSTEM) \
    $(if $(filter parallel,$(MODULES)),,-DSOLO_NO_PARALLEL) \
    $(if $(filter serial,$(MODULES)),,-DSOLO_NO_SERIAL) \
    $(if $(filter socket,$(MODULES)),,-DSOLO_NO_SOCKET) \
    $(if $(filter sandbox,$(MODULES)),,-DSOLO_NO_SANDBOX)

DOS_WATT32_FLAGS = $(if $(filter socket,$(MODULES)),-DUSE_WATT32 -I$(WATT32_DIR)/inc)
DOS_CFLAGS ?= -bt=dos -zq -wx $(DOS_WATT32_FLAGS) $(SOLO_MODULE_DEFS) -Iinclude

DOS_SRCS = \
    src/solo.c \
    $(if $(filter platform,$(MODULES)),src/platform/platform.c) \
    $(if $(filter filesystem,$(MODULES)),src/filesystem/filesystem.c) \
    $(if $(filter parallel,$(MODULES)),src/parallel/parallel_dos.c) \
    $(if $(filter serial,$(MODULES)),src/serial/serial_dos.c) \
    $(if $(filter socket,$(MODULES)),src/socket/socket_dos.c) \
    $(if $(filter sandbox,$(MODULES)),src/sandbox/sandbox_stub.c)

DOS_OBJS = $(patsubst src/%.c,$(DOS_BUILD_DIR)/%.obj,$(DOS_SRCS))

.PHONY: dos
dos: $(DOS_BUILD_DIR)/libsolo.lib

$(DOS_BUILD_DIR)/libsolo.lib: $(DOS_OBJS)
	@mkdir -p $(dir $@)
	$(DOS_WLIB) -q -b -c -n $@ $(foreach obj,$(DOS_OBJS),+$(obj))
	@echo "==> Built MS-DOS static library: $@"

$(DOS_BUILD_DIR)/%.obj: src/%.c
	@mkdir -p $(dir $@)
	$(DOS_WCC) $(DOS_CFLAGS) $< -fo=$@
