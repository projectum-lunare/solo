# mk/windows_ce.mk - Windows CE ARM Target (MS eVC 3.0 clarm via wibo)

WIBO_BIN ?= $(HOME)/.local/opt/wibo/bin/wibo
EVC3_DIR ?= $(HOME)/.local/opt/evc3

WINCE_BUILD_DIR ?= build/windows_ce

WINCE_CC ?= $(WIBO_BIN) $(EVC3_DIR)/bin/clarm.exe
WINCE_AR ?= ar

WINCE_CFLAGS ?= -c -nologo -O2 -Iinclude -I$(EVC3_DIR)/include -D_WIN32_WCE=211 -DUNDER_CE=211 -DARM -D_ARM_

WINCE_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_windows_ce.c \
    src/serial/serial_windows_ce.c \
    src/socket/socket_windows_ce.c \
    src/sandbox/sandbox_stub.c

WINCE_OBJS = $(patsubst src/%.c,$(WINCE_BUILD_DIR)/%.obj,$(WINCE_SRCS))

.PHONY: wince windows_ce
wince: $(WINCE_BUILD_DIR)/libsolo.a
windows_ce: wince

$(WINCE_BUILD_DIR)/libsolo.a: $(WINCE_OBJS)
	@mkdir -p $(dir $@)
	$(WINCE_AR) rcs $@ $^
	@echo "==> Built Windows CE static library: $@"

$(WINCE_BUILD_DIR)/%.obj: src/%.c
	@mkdir -p $(dir $@)
	$(WINCE_CC) $(WINCE_CFLAGS) -Fo$@ $<
