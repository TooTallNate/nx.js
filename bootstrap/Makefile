#---------------------------------------------------------------------------------
# nx.js slim bootstrap launcher — a tiny homebrew NRO that chainloads the shared
# nx.js runtime from sdmc:/nx.js/. Builds with the standard devkitPro switch
# rules (libnx only — no V8/Skia), so the output is a few KB of code.
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

export PATH := $(DEVKITPRO)/devkitA64/bin:$(DEVKITPRO)/tools/bin:$(PATH)

# Absolute path to THIS Makefile's directory — captured before the switch_rules
# include changes MAKEFILE_LIST, and valid in both the top-level and recursive
# (build/) make invocations (where CURDIR changes).
MK_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# Metadata. APP_VERSION + the default runtime requirement track the runtime's
# semver MAJOR (e.g. "1.0.0-beta.1" -> major "1" -> default "^1").
#---------------------------------------------------------------------------------
RUNTIME_PKG := $(MK_DIR)/../packages/runtime/package.json
APP_TITLE   :=  nx.js app
APP_AUTHOR  :=  TooTallNate
APP_VERSION :=  `jq -r .version < $(RUNTIME_PKG)`
# Use the shared nx.js default icon (the same icon.jpg embedded in nxjs.nro at
# the repo root) for the slim launcher base, rather than libnx's default icon.
# ICON is relative to this project dir (the devkitPro rules prepend TOPDIR).
ICON        :=  ../icon.jpg
RUNTIME_MAJOR := $(shell jq -r .version < $(RUNTIME_PKG) | sed 's/[.-].*//')
# Default [runtime] version requirement baked into the launcher: caret-on-major.
NXJS_RUNTIME_DEFAULT := ^$(RUNTIME_MAJOR)

TARGET		:=	bootstrap
BUILD		:=	build
SOURCES		:=	source source/vendor
INCLUDES	:=	source

#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(ARCH) $(DEFINES) \
			-DNXJS_RUNTIME_DEFAULT="\"$(NXJS_RUNTIME_DEFAULT)\""
CFLAGS	+=	$(INCLUDE) -D__SWITCH__ -D_DEFAULT_SOURCE

CXXFLAGS	:=	$(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:=	-lnx
LIBDIRS	:=	$(PORTLIBS) $(LIBNX)

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
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES	:=	$(OFILES_SRC)

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

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf

#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

all	:	$(OUTPUT).nro

$(OUTPUT).nro	:	$(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	:

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
