# mk/windows_nt.mk - Windows NT Multi-Architecture Target (VC4 & MinGW)
# Supported toolchains & architectures:
#   - VC4 (MSVC 4.x via wibo/runner): Alpha, MIPS, PowerPC, x86
#   - MinGW (GCC): x86 (x86_64-w64-mingw32), ARM (aarch64-w64-mingw32)

WIBO_BIN ?= $(HOME)/.local/opt/wibo/bin/wibo
VC4_DIR  ?= $(HOME)/.local/opt/vc4

WINNT_BUILD_DIR ?= build/windows_nt
WINNT_AR        ?= ar

ALL_MODULES ?= platform filesystem parallel serial socket sandbox
MODULES     ?= $(ALL_MODULES)
SOLO_MODULE_DEFS ?= \
    $(if $(filter platform,$(MODULES)),,-DSOLO_NO_PLATFORM) \
    $(if $(filter filesystem,$(MODULES)),,-DSOLO_NO_FILESYSTEM) \
    $(if $(filter parallel,$(MODULES)),,-DSOLO_NO_PARALLEL) \
    $(if $(filter serial,$(MODULES)),,-DSOLO_NO_SERIAL) \
    $(if $(filter socket,$(MODULES)),,-DSOLO_NO_SOCKET) \
    $(if $(filter sandbox,$(MODULES)),,-DSOLO_NO_SANDBOX)

# Sources
WINNT_SRCS = \
    src/solo.c \
    $(if $(filter platform,$(MODULES)),src/platform/platform.c) \
    $(if $(filter filesystem,$(MODULES)),src/filesystem/filesystem.c) \
    $(if $(filter parallel,$(MODULES)),src/parallel/parallel_windows_nt.c) \
    $(if $(filter serial,$(MODULES)),src/serial/serial_windows_nt.c) \
    $(if $(filter socket,$(MODULES)),src/socket/socket_windows_nt.c) \
    $(if $(filter sandbox,$(MODULES)),src/sandbox/sandbox_stub.c)

# -----------------------------------------------------------------------------
# Microsoft Visual C++ 4.x (RISC & x86) Configuration
# -----------------------------------------------------------------------------
VC4_RUNNER ?= $(WIBO_BIN)
VC4_RUNNER_alpha ?= $(VC4_RUNNER)
VC4_RUNNER_mips  ?= $(VC4_RUNNER)
VC4_RUNNER_ppc   ?= $(VC4_RUNNER)
VC4_RUNNER_x86   ?= $(VC4_RUNNER)

VC4_COMMON_CFLAGS = -c -nologo -O2 -D_WIN32 -D_CONSOLE $(SOLO_MODULE_DEFS) -Iinclude

VC4_CC_alpha ?= $(VC4_RUNNER_alpha) $(VC4_DIR)/alpha/bin/cl.exe
VC4_CFLAGS_alpha ?= $(VC4_COMMON_CFLAGS) -I$(VC4_DIR)/alpha/include -D_ALPHA_ -D_M_ALPHA

VC4_CC_mips ?= $(VC4_RUNNER_mips) $(VC4_DIR)/mips/bin/cl.exe
VC4_CFLAGS_mips ?= $(VC4_COMMON_CFLAGS) -I$(VC4_DIR)/mips/include -DMIPS -D_MIPS_ -D_M_MRX000

VC4_CC_ppc ?= $(VC4_RUNNER_ppc) $(VC4_DIR)/ppc/bin/cl.exe
VC4_CFLAGS_ppc ?= $(VC4_COMMON_CFLAGS) -I$(VC4_DIR)/ppc/include -DPPC -D_PPC_ -D_M_PPC

VC4_CC_x86 ?= $(VC4_RUNNER_x86) $(VC4_DIR)/x86/bin/cl.exe
VC4_CFLAGS_x86 ?= $(VC4_COMMON_CFLAGS) -I$(VC4_DIR)/x86/include -D_X86_ -D_M_IX86=400

define VC4_RULES
WINNT_VC4_$(1)_OBJS = $$(patsubst src/%.c,$$(WINNT_BUILD_DIR)/vc4/$(1)/%.obj,$$(WINNT_SRCS))

.PHONY: winnt-vc4-$(1) winnt-$(1)
winnt-vc4-$(1): $$(WINNT_BUILD_DIR)/vc4/$(1)/libsolo.a
winnt-$(1): winnt-vc4-$(1)

$$(WINNT_BUILD_DIR)/vc4/$(1)/libsolo.a: $$(WINNT_VC4_$(1)_OBJS)
	@mkdir -p $$(dir $$@)
	$$(WINNT_AR) rcs $$@ $$^
	@echo "==> Built Windows NT VC4 $(1) static library: $$@"

$$(WINNT_BUILD_DIR)/vc4/$(1)/%.obj: src/%.c
	@mkdir -p $$(dir $$@)
	$$(VC4_CC_$(1)) $$(VC4_CFLAGS_$(1)) -Fo$$@ $$<
endef

$(eval $(call VC4_RULES,alpha))
$(eval $(call VC4_RULES,mips))
$(eval $(call VC4_RULES,ppc))
$(eval $(call VC4_RULES,x86))

.PHONY: winnt-vc4 windows_nt-vc4
winnt-vc4: winnt-vc4-alpha winnt-vc4-mips winnt-vc4-ppc winnt-vc4-x86
windows_nt-vc4: winnt-vc4

# -----------------------------------------------------------------------------
# MinGW-w64 (GCC) Configuration
# -----------------------------------------------------------------------------
MINGW_CC_x86 ?= x86_64-w64-mingw32-gcc
MINGW_AR_x86 ?= x86_64-w64-mingw32-ar
MINGW_CFLAGS_x86 ?= -Wall -Wextra -O2 -std=c89 -D_WIN32 -D_WIN64 $(SOLO_MODULE_DEFS) -Iinclude

MINGW_CC_arm ?= aarch64-w64-mingw32-gcc
MINGW_AR_arm ?= aarch64-w64-mingw32-ar
MINGW_CFLAGS_arm ?= -Wall -Wextra -O2 -std=c89 -D_WIN32 -D_WIN64 -D_ARM64_ $(SOLO_MODULE_DEFS) -Iinclude

define MINGW_RULES
WINNT_MINGW_$(1)_OBJS = $$(patsubst src/%.c,$$(WINNT_BUILD_DIR)/mingw/$(1)/%.o,$$(WINNT_SRCS))

.PHONY: winnt-mingw-$(1)
winnt-mingw-$(1): $$(WINNT_BUILD_DIR)/mingw/$(1)/libsolo.a

$$(WINNT_BUILD_DIR)/mingw/$(1)/libsolo.a: $$(WINNT_MINGW_$(1)_OBJS)
	@mkdir -p $$(dir $$@)
	$$(if $$(shell which $$(MINGW_AR_$(1)) 2>/dev/null),$$(MINGW_AR_$(1)),$$(WINNT_AR)) rcs $$@ $$^
	@echo "==> Built Windows NT MinGW $(1) static library: $$@"

$$(WINNT_BUILD_DIR)/mingw/$(1)/%.o: src/%.c
	@mkdir -p $$(dir $$@)
	$$(MINGW_CC_$(1)) $$(MINGW_CFLAGS_$(1)) -c $$< -o $$@
endef

$(eval $(call MINGW_RULES,x86))
$(eval $(call MINGW_RULES,arm))

.PHONY: winnt-mingw windows_nt-mingw winnt-arm
winnt-mingw: winnt-mingw-x86 winnt-mingw-arm
windows_nt-mingw: winnt-mingw
winnt-arm: winnt-mingw-arm

# -----------------------------------------------------------------------------
# Umbrella Targets
# -----------------------------------------------------------------------------
WINNT_TARGETS ?= winnt-vc4-x86

.PHONY: winnt windows_nt winnt-all windows_nt-all
winnt: $(WINNT_TARGETS)
	@mkdir -p $(WINNT_BUILD_DIR)
	@if [ -f $(WINNT_BUILD_DIR)/vc4/x86/libsolo.a ]; then \
		cp -f $(WINNT_BUILD_DIR)/vc4/x86/libsolo.a $(WINNT_BUILD_DIR)/libsolo.a; \
	fi

windows_nt: winnt

winnt-all: winnt-vc4 winnt-mingw
windows_nt-all: winnt-all
