# mk/os2.mk - OS/2 32-bit Target (Open Watcom)

WATCOM_DIR ?= $(HOME)/.local/opt/watcom

OS2_BUILD_DIR ?= build/os2

OS2_WCC  ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH EDPATH=$(WATCOM_DIR)/eddat INCLUDE=$(WATCOM_DIR)/h:$(WATCOM_DIR)/h/os2 $(WATCOM_DIR)/binl64/wcc386
OS2_WLIB ?= WATCOM=$(WATCOM_DIR) PATH=$(WATCOM_DIR)/binl64:$$PATH $(WATCOM_DIR)/binl64/wlib

OS2_CFLAGS ?= -bt=os2 -zq -wx -Iinclude

OS2_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_os2.c \
    src/serial/serial_os2.c \
    src/socket/socket_os2.c \
    src/sandbox/sandbox_stub.c

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
