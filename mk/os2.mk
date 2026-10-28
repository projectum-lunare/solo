# mk/os2.mk - OS/2 Target (Open Watcom)

WATCOM_DIR ?= $(HOME)/.local/opt/watcom

OS2_BUILD_DIR ?= build/os2

OS2_WCC  ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH EDPATH=$(WATCOM_DIR)/eddat INCLUDE=$(WATCOM_DIR)/h:$(WATCOM_DIR)/h/os2 $(WATCOM_DIR)/binl64/wcc386
OS2_WLIB ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH $(WATCOM_DIR)/binl64/wlib

ALL_MODULES ?= platform filesystem parallel serial socket sandbox
MODULES     ?= $(ALL_MODULES)
SOLO_MODULE_DEFS ?= \
    $(if $(filter platform,$(MODULES)),,-DSOLO_NO_PLATFORM) \
    $(if $(filter filesystem,$(MODULES)),,-DSOLO_NO_FILESYSTEM) \
    $(if $(filter parallel,$(MODULES)),,-DSOLO_NO_PARALLEL) \
    $(if $(filter serial,$(MODULES)),,-DSOLO_NO_SERIAL) \
    $(if $(filter socket,$(MODULES)),,-DSOLO_NO_SOCKET) \
    $(if $(filter sandbox,$(MODULES)),,-DSOLO_NO_SANDBOX)

OS2_CFLAGS ?= -bt=os2 -zq -wx $(SOLO_MODULE_DEFS) -Iinclude

OS2_SRCS = \
    src/solo.c \
    $(if $(filter platform,$(MODULES)),src/platform/platform.c) \
    $(if $(filter filesystem,$(MODULES)),src/filesystem/filesystem.c) \
    $(if $(filter parallel,$(MODULES)),src/parallel/parallel_os2.c) \
    $(if $(filter serial,$(MODULES)),src/serial/serial_os2.c) \
    $(if $(filter socket,$(MODULES)),src/socket/socket_os2.c) \
    $(if $(filter sandbox,$(MODULES)),src/sandbox/sandbox_stub.c)

OS2_OBJS = $(patsubst src/%.c,$(OS2_BUILD_DIR)/%.obj,$(OS2_SRCS))

.PHONY: os2
os2: $(OS2_BUILD_DIR)/libsolo.lib

$(OS2_BUILD_DIR)/libsolo.lib: $(OS2_OBJS)
	@mkdir -p $(dir $@)
	$(OS2_WLIB) -q -b -c -n $@ $(foreach obj,$(OS2_OBJS),+$(obj))
	@echo "==> Built OS/2 static library: $@"

$(OS2_BUILD_DIR)/%.obj: src/%.c
	@mkdir -p $(dir $@)
	$(OS2_WCC) $(OS2_CFLAGS) $< -fo=$@
