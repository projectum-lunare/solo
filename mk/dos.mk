# mk/dos.mk - MS-DOS Target (Open Watcom + WATT-32)

WATCOM_DIR ?= $(HOME)/.local/opt/watcom
WATT32_DIR ?= $(HOME)/.local/opt/watt32

DOS_BUILD_DIR ?= build/dos

DOS_WCC  ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH EDPATH=$(WATCOM_DIR)/eddat INCLUDE=$(WATCOM_DIR)/h:$(WATCOM_DIR)/h/dos $(WATCOM_DIR)/binl64/wcc386
DOS_WLIB ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH $(WATCOM_DIR)/binl64/wlib

DOS_CFLAGS ?= -bt=dos -zq -wx -DUSE_WATT32 -I$(WATT32_DIR)/inc -Iinclude

DOS_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_dos.c \
    src/serial/serial_dos.c \
    src/socket/socket_dos.c \
    src/sandbox/sandbox_stub.c

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
