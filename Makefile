#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

# Ensure the devkitPro toolchain dirs are on PATH so callers only need to set
# DEVKITPRO. Covers aarch64-none-elf-pkg-config (portlibs), the devkitA64
# compilers, and tools like elf2nro. Exported so it applies to every recipe
# shell (e.g. the backtick pkg-config calls in CFLAGS/LIBS) below.
#
# NOTE: GNU Make's parse-time $(shell ...) does NOT see this exported PATH, so
# parse-time tool lookups (UV_CFLAGS/UV_LIBS) use $(PKG_CONFIG) with an explicit
# path instead.
export PATH := $(DEVKITPRO)/portlibs/switch/bin:$(DEVKITPRO)/devkitA64/bin:$(DEVKITPRO)/tools/bin:$(PATH)
PKG_CONFIG := $(DEVKITPRO)/portlibs/switch/bin/aarch64-none-elf-pkg-config

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
# EXEFS_SRC is the optional input directory containing data copied into exefs, if anything this normally should only contain "main.npdm".
# ROMFS is the directory containing data to be added to RomFS, relative to the Makefile (Optional)
#
# NO_ICON: if set to anything, do not use icon.
# NO_NACP: if set to anything, no .nacp file is generated.
# APP_TITLE is the name of the app stored in the .nacp file (Optional)
# APP_AUTHOR is the author of the app stored in the .nacp file (Optional)
# APP_VERSION is the version of the app stored in the .nacp file (Optional)
# APP_TITLEID is the titleID of the app stored in the .nacp file (Optional)
# ICON is the filename of the icon (.jpg), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.jpg
#     - icon.jpg
#     - <libnx folder>/default_icon.jpg
#---------------------------------------------------------------------------------

APP_TITLE   :=  nx.js
APP_TITLEID :=  016e782e6a730000 # 6e782e6a73 is "nx.js" in hex 😉
APP_AUTHOR  :=  TooTallNate
APP_VERSION :=  `jq -r .version < ../packages/runtime/package.json`
LIBNX_VERSION := $(shell $(DEVKITPRO)/pacman/bin/pacman -Q libnx 2>/dev/null | cut -d' ' -f2 | cut -d'-' -f1)
LIBTURBOJPEG_VERSION := $(shell $(DEVKITPRO)/pacman/bin/pacman -Q switch-libjpeg-turbo 2>/dev/null | cut -d' ' -f2 | cut -d'-' -f1)

TARGET		:=	nxjs
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include
EXEFS_SRC	:=	exefs_src
ROMFS		:=	romfs
CONFIG_JSON	:=	npdm.json

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

# V8 must NOT see V8_COMPRESS_POINTERS defined (checked by presence). libuv's
# pkg-config force-includes nx-prelude.h and adds _GNU_SOURCE / SSIZE_MAX.
UV_CFLAGS	:=	$(shell $(PKG_CONFIG) --cflags libuv)
UV_LIBS		:=	$(shell $(PKG_CONFIG) --libs libuv)

CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(ARCH) $(DEFINES) -DNXJS_VERSION="\"${APP_VERSION}\"" \
			-DLIBNX_VERSION="\"$(LIBNX_VERSION)\"" \
			-DLIBTURBOJPEG_VERSION="\"$(LIBTURBOJPEG_VERSION)\""

CFLAGS	+=	$(INCLUDE) -D__SWITCH__ -D_DEFAULT_SOURCE $(UV_CFLAGS) \
			`$(PKG_CONFIG) freetype2 --cflags`

# Skia (Ganesh GL). Source-root include dir; SK_GL selects the GL backend.
SKIA_CFLAGS	:=	-DSK_GL -DSK_BUILD_FOR_UNIX -I$(PORTLIBS)/include/skia

# V8's API is C++; nx.js native modules are C++ (.cc), no exceptions/RTTI, C++20.
# Third-party header noise we can't fix in our code:
#   -Wno-comment: V8 headers (v8-internal.h) contain ASCII-art block comments.
#   -Wno-attributes: Skia's SkTemplates.h uses the clang-only `clang::reinitializes`
#     scoped attribute, which devkitPro GCC doesn't recognize.
#   -Wno-strict-aliasing: V8's v8-function-callback.h PropertyCallbackInfo::
#     GetIsolate() does a reinterpret_cast type-pun in the engine headers.
CXXFLAGS	:=	$(CFLAGS) $(SKIA_CFLAGS) -fno-rtti -fno-exceptions -std=c++20 \
			-Wno-comment -Wno-attributes -Wno-strict-aliasing

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=${DEVKITPRO}/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

# V8 static libs must be linked in a --start-group (circular refs). The patched
# archive in $(BUILD) (libc-horizon symbols weakened) takes precedence via -L.
V8_LIBS	:=	-Wl,--start-group -lv8_monolith -labsl -lchrome_zlib -lcompression_utils_portable -Wl,--end-group

# Skia (Ganesh GL) link stack: the horizon port object, then the GL-suffixed
# Skia archives in a group, then the EGL/GLES/Mesa stack. Matches the validated
# trifecta link recipe (MIGRATION-NOTES §269-438).
SKIA_LIBS	:=	$(PORTLIBS)/lib/skia-horizon-port.o \
			-Wl,--start-group -lskia-gl -lskcms-gl -lskshaper-gl \
			-lskunicode_core-gl -lskunicode_libgrapheme-gl -Wl,--end-group
GL_LIBS		:=	-lEGL -lGLESv2 -lglapi -ldrm_nouveau

# Link order (proven): Skia -> V8 -> libuv -> GL -> freetype/harfbuzz -> codecs.
# -lharfbuzz (system) satisfies switch-freetype's autohinter; Skia bundles its
# own ICU-free HarfBuzz internally. png/z are explicit (were formerly pulled via
# cairo). Cairo and pixman are fully removed in Phase 2.1 (Skia is the sole
# rendering backend).
# ffmpeg (media decode for the Video element + decodeAudioData): demux/codec/
# scale/resample only (no avfilter/avdevice). dav1d backs ffmpeg's AV1 decode.
FFMPEG_LIBS	:=	-lavformat -lavcodec -lswscale -lswresample -lavutil -ldav1d

LIBS	:=  $(SKIA_LIBS) $(V8_LIBS) $(UV_LIBS) $(GL_LIBS) \
			$(FFMPEG_LIBS) \
			-lmbedtls -lmbedx509 -lmbedcrypto \
			-Wl,--start-group -lfreetype -lharfbuzz -Wl,--end-group \
			-lturbojpeg -lwebp -lwebpdemux -ljpeg -lpng -lbz2 -lz -lm -lzstd \
			-lada \
			-L$(DEVKITPRO)/libnx/lib -lnx

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) $(LIBNX)


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
CCFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cc)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

# Add `runtime_js.c` (embedded runtime.js) to CFILES if necessary (i.e. on CI)
ifneq ($(filter runtime_js.c,$(CFILES)),runtime_js.c)
	CFILES := $(CFILES) runtime_js.c
endif

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)$(CCFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CCFILES:.cc=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export BUILD_EXEFS_SRC := $(TOPDIR)/$(EXEFS_SRC)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.jpg)
	ifneq (,$(findstring $(TARGET).jpg,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).jpg
	else
		ifneq (,$(findstring icon.jpg,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.jpg
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_ICON)),)
	export NROFLAGS += --icon=$(APP_ICON)
endif

ifeq ($(strip $(NO_NACP)),)
	export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp
endif

ifneq ($(APP_TITLEID),)
	export NACPFLAGS += --titleid=$(APP_TITLEID)
endif

ifneq ($(ROMFS),)
	export NROFLAGS += --romfsdir=$(CURDIR)/$(ROMFS)
endif

ifeq ($(strip $(CONFIG_JSON)),)
	jsons := $(wildcard *.json)
	ifneq (,$(findstring $(TARGET).json,$(jsons)))
		export APP_JSON := $(TOPDIR)/$(TARGET).json
	else
		ifneq (,$(findstring config.json,$(jsons)))
			export APP_JSON := $(TOPDIR)/config.json
		endif
	endif
else
	export APP_JSON := $(TOPDIR)/$(CONFIG_JSON)
endif

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

# Embed runtime.js as a C byte array (replaces the old qjsc bytecode step).
# V8 evaluates the runtime from source at boot.
$(SOURCES)/runtime_js.c: packages/runtime/runtime.js tools/embed-runtime.mjs
	@node tools/embed-runtime.mjs packages/runtime/runtime.js $(SOURCES)/runtime_js.c
	@echo "embedded 'packages/runtime/runtime.js' -> '$(SOURCES)/runtime_js.c'"

$(ROMFS)/runtime.js.map: packages/runtime/runtime.js.map
	@mkdir -p $(ROMFS)
	@cp -v packages/runtime/runtime.js.map $(ROMFS)

# Geist Mono font, used by the canvas-backed console renderer. Extracted at
# build time from the `geist` npm package (a devDependency of @nx.js/runtime)
# into romfs (mounted as `nxjs:/GeistMono.ttf` at runtime) — NOT committed to
# the repo. The pnpm symlink at packages/runtime/node_modules/geist is a stable
# path to the versioned package in the store.
GEIST_MONO_TTF := packages/runtime/node_modules/geist/dist/fonts/geist-mono/GeistMono-Regular.ttf
$(ROMFS)/GeistMono.ttf: $(GEIST_MONO_TTF)
	@mkdir -p $(ROMFS)
	@cp -v $(GEIST_MONO_TTF) $(ROMFS)/GeistMono.ttf

$(BUILD): source/runtime_js.c romfs/runtime.js.map romfs/GeistMono.ttf
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(SOURCES)/runtime_js.c $(TARGET).pfs0 $(TARGET).nso $(TARGET).nro $(TARGET).nacp $(TARGET).elf $(TARGET).npdm


#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all	: $(OUTPUT).pfs0 $(OUTPUT).nro

ifeq ($(strip $(APP_JSON)),)
$(OUTPUT).pfs0	:	$(OUTPUT).nso
else
$(OUTPUT).pfs0	:	$(OUTPUT).nso $(OUTPUT).npdm
endif

$(OUTPUT).nso	:	$(OUTPUT).elf

ifeq ($(strip $(NO_NACP)),)
$(OUTPUT).nro	:	$(OUTPUT).elf $(OUTPUT).nacp
else
$(OUTPUT).nro	:	$(OUTPUT).elf
endif

$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	: $(HFILES_BIN)

#---------------------------------------------------------------------------------
# devkitA64 base_rules only defines %.o: %.cpp; nx.js native modules are .cc.
#---------------------------------------------------------------------------------
%.o: %.cc
	@echo $(notdir $<)
	$(SILENTCMD)$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@ $(ERROR_FILTER)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
