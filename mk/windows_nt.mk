# mk/windows_nt.mk - Windows NT Target (Open Watcom Win32)

WATCOM_DIR ?= $(HOME)/.local/opt/watcom

WINNT_BUILD_DIR ?= build/windows_nt

WINNT_WCC  ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH EDPATH=$(WATCOM_DIR)/eddat INCLUDE=$(WATCOM_DIR)/h:$(WATCOM_DIR)/h/nt $(WATCOM_DIR)/binl64/wcc386
WINNT_WLIB ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH $(WATCOM_DIR)/binl64/wlib

ALL_MODULES ?= platform filesystem parallel serial socket sandbox
MODULES     ?= $(ALL_MODULES)
SOLO_MODULE_DEFS ?= \
    $(if $(filter platform,$(MODULES)),,-DSOLO_NO_PLATFORM) \
    $(if $(filter filesystem,$(MODULES)),,-DSOLO_NO_FILESYSTEM) \
    $(if $(filter parallel,$(MODULES)),,-DSOLO_NO_PARALLEL) \
    $(if $(filter serial,$(MODULES)),,-DSOLO_NO_SERIAL) \
    $(if $(filter socket,$(MODULES)),,-DSOLO_NO_SOCKET) \
    $(if $(filter sandbox,$(MODULES)),,-DSOLO_NO_SANDBOX)

WINNT_CFLAGS ?= -bt=nt -zq -wx $(SOLO_MODULE_DEFS) -Iinclude

WINNT_SRCS = \
    src/solo.c \
    $(if $(filter platform,$(MODULES)),src/platform/platform.c) \
    $(if $(filter filesystem,$(MODULES)),src/filesystem/filesystem.c) \
    $(if $(filter parallel,$(MODULES)),src/parallel/parallel_windows_nt.c) \
    $(if $(filter serial,$(MODULES)),src/serial/serial_windows_nt.c) \
    $(if $(filter socket,$(MODULES)),src/socket/socket_windows_nt.c) \
    $(if $(filter sandbox,$(MODULES)),src/sandbox/sandbox_stub.c)

WINNT_OBJS = $(patsubst src/%.c,$(WINNT_BUILD_DIR)/%.obj,$(WINNT_SRCS))

.PHONY: winnt windows_nt
winnt: $(WINNT_BUILD_DIR)/libsolo.lib
windows_nt: winnt

$(WINNT_BUILD_DIR)/libsolo.lib: $(WINNT_OBJS)
	@mkdir -p $(dir $@)
	$(WINNT_WLIB) -q -b -c -n $@ $(foreach obj,$(WINNT_OBJS),+$(obj))
	@echo "==> Built Windows NT static library: $@"

$(WINNT_BUILD_DIR)/%.obj: src/%.c
	@mkdir -p $(dir $@)
	$(WINNT_WCC) $(WINNT_CFLAGS) $< -fo=$@
