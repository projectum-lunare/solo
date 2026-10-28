# mk/windows_ce.mk - Windows CE Multi-Architecture Target (MS eVC 3.0 via wibo)
# Supported architectures: ARM, MIPS, PowerPC, SH-3, SH-4, x86

WIBO_BIN ?= $(HOME)/.local/opt/wibo/bin/wibo
EVC3_DIR ?= $(HOME)/.local/opt/evc3

WINCE_BUILD_DIR ?= build/windows_ce
WINCE_AR        ?= ar

WINCE_COMMON_CFLAGS = -c -nologo -O2 -Iinclude -I$(EVC3_DIR)/include -D_WIN32_WCE=211 -DUNDER_CE=211

# Architecture compilers & CFLAGS
WINCE_ARM_CC      ?= $(WIBO_BIN) $(EVC3_DIR)/bin/clarm.exe
WINCE_ARM_CFLAGS  ?= $(WINCE_COMMON_CFLAGS) -DARM -D_ARM_ -DARMV4

WINCE_MIPS_CC     ?= $(WIBO_BIN) $(EVC3_DIR)/bin/clmips.exe
WINCE_MIPS_CFLAGS ?= $(WINCE_COMMON_CFLAGS) -DMIPS -D_MIPS_ -DMIPS16 -DMIPS16SUPPORT -DR4000 -QMmips16

WINCE_PPC_CC      ?= $(WIBO_BIN) $(EVC3_DIR)/bin/clppc.exe
WINCE_PPC_CFLAGS  ?= $(WINCE_COMMON_CFLAGS) -DPPC -D_PPC_

WINCE_SH3_CC      ?= $(WIBO_BIN) $(EVC3_DIR)/bin/shcl.exe
WINCE_SH3_CFLAGS  ?= $(WINCE_COMMON_CFLAGS) -DSHx -D_SH3_ -Qsh3

WINCE_SH4_CC      ?= $(WIBO_BIN) $(EVC3_DIR)/bin/shcl.exe
WINCE_SH4_CFLAGS  ?= $(WINCE_COMMON_CFLAGS) -DSHx -D_SH4_ -Qsh4

WINCE_X86_CC      ?= $(WIBO_BIN) $(EVC3_DIR)/bin/cl.exe
WINCE_X86_CFLAGS  ?= $(WINCE_COMMON_CFLAGS) -Dx86 -D_X86_ -D_WIN32_WCE_EMULATION

WINCE_SRCS = \
    src/solo.c \
    src/platform/platform.c \
    src/filesystem/filesystem.c \
    src/parallel/parallel_windows_ce.c \
    src/serial/serial_windows_ce.c \
    src/socket/socket_windows_ce.c \
    src/sandbox/sandbox_stub.c

define WINCE_RULES
WINCE_$(1)_OBJS = $$(patsubst src/%.c,$$(WINCE_BUILD_DIR)/$(1)/%.obj,$$(WINCE_SRCS))

.PHONY: wince-$(1)
wince-$(1): $$(WINCE_BUILD_DIR)/$(1)/libsolo.a

$$(WINCE_BUILD_DIR)/$(1)/libsolo.a: $$(WINCE_$(1)_OBJS)
	@mkdir -p $$(dir $$@)
	$$(WINCE_AR) rcs $$@ $$^
	@echo "==> Built Windows CE $(1) static library: $$@"

$$(WINCE_BUILD_DIR)/$(1)/%.obj: src/%.c
	@mkdir -p $$(dir $$@)
	$$(WINCE_$(2)_CC) $$(WINCE_$(2)_CFLAGS) -Fo$$@ $$<
endef

$(eval $(call WINCE_RULES,arm,ARM))
$(eval $(call WINCE_RULES,mips,MIPS))
$(eval $(call WINCE_RULES,ppc,PPC))
$(eval $(call WINCE_RULES,sh3,SH3))
$(eval $(call WINCE_RULES,sh4,SH4))
$(eval $(call WINCE_RULES,x86,X86))

.PHONY: wince windows_ce wince-all
wince: wince-arm wince-mips wince-ppc wince-sh3 wince-sh4 wince-x86 $(WINCE_BUILD_DIR)/libsolo.a
windows_ce: wince
wince-all: wince

$(WINCE_BUILD_DIR)/libsolo.a: $(WINCE_BUILD_DIR)/arm/libsolo.a
	@mkdir -p $(dir $@)
	@cp $< $@
