# mk/windows_nt.mk - Windows NT Target (Open Watcom Win32)

WATCOM_DIR ?= $(HOME)/.local/opt/watcom

WINNT_BUILD_DIR ?= build/windows_nt

WINNT_WCC  ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH EDPATH=$(WATCOM_DIR)/eddat INCLUDE=$(WATCOM_DIR)/h:$(WATCOM_DIR)/h/nt $(WATCOM_DIR)/binl64/wcc386
WINNT_WLIB ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH $(WATCOM_DIR)/binl64/wlib

WINNT_CFLAGS ?= -bt=nt -zq -wx -Iinclude

WINNT_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_windows_nt.c \
    src/serial/serial_windows_nt.c \
    src/socket/socket_windows_nt.c \
    src/sandbox/sandbox_stub.c

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
