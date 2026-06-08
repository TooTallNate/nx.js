#---------------------------------------------------------------------------------
# Shared devkitPro build scaffold for the nx.js bootstrap launchers.
#
# Included by bootstrap/launcher-nro/Makefile and bootstrap/launcher-nsp/Makefile.
# Each launcher sets, BEFORE including this file:
#   TARGET        - output base name (e.g. "bootstrap" or "hbl")
#   NXJS_OUTPUT   - "nro" (homebrew NRO) or "nsp" (exefs NSO + NPDM)
#   CONFIG_JSON   - (nsp only) the NPDM json filename
#   ICON          - (nro only, optional) icon path relative to the launcher dir
# and may append extra LDFLAGS/LIBS/SOURCES as needed.
#
# The COMMON source lives one level up in bootstrap/source/ (resolve, ui, match,
# vendored ini + semver) and is compiled into BOTH launchers; only each
# launcher's own source/ (the final "launch" step) differs.
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

export PATH := $(DEVKITPRO)/devkitA64/bin:$(DEVKITPRO)/tools/bin:$(PATH)

# Absolute path to THIS file's directory (bootstrap/), captured before the
# switch_rules include changes MAKEFILE_LIST. Valid in both the top-level and
# recursive (build/) make invocations.
COMMON_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

TOPDIR ?= $(CURDIR)

#---------------------------------------------------------------------------------
# Metadata. APP_VERSION + the default runtime requirement track the runtime's
# semver MAJOR (e.g. "1.0.0-beta.1" -> major "1" -> default "^1").
#
# These MUST be set before including switch_rules: that file resolves APP_TITLE/
# APP_AUTHOR to their own defaults ("$(notdir OUTPUT)" / "Unspecified Author")
# at include time, so a later `?=` here would be a no-op and the launcher NACP
# would carry the wrong title/author. APP_TITLEID matches the root Makefile's
# nx.js title id so a slim app's NACP inherits the same PresenceGroupId as the
# fat build (Application.self.id then matches across packaging modes).
#---------------------------------------------------------------------------------
RUNTIME_PKG := $(COMMON_DIR)/../packages/runtime/package.json
APP_TITLE   ?=  nx.js app
APP_AUTHOR  ?=  TooTallNate
APP_TITLEID ?=  016e782e6a730000
APP_VERSION :=  `jq -r .version < $(RUNTIME_PKG)`
# Default [runtime] version requirement baked into the launcher: caret on the
# FULL runtime version this launcher was built against (e.g. ^1.0.0-beta.2),
# i.e. "at least the version packaged with, or any newer compatible release".
# A caret-on-major (^1) would wrongly accept an OLDER runtime that lacks
# features the app expects. This is only the fallback for a hand-built app whose
# nxjs.ini has no [runtime] version; @nx.js/nro and @nx.js/nsp inject their own.
NXJS_RUNTIME_DEFAULT := ^$(shell jq -r .version < $(RUNTIME_PKG))

# The devkitPro %.nacp rule passes APP_TITLEID only via NACPFLAGS (not as a
# positional arg), so wire it through like the root Makefile does. This bakes
# the PresenceGroupId into the launcher NACP, which a slim app inherits.
ifneq ($(APP_TITLEID),)
	export NACPFLAGS += --titleid=$(APP_TITLEID)
endif

include $(DEVKITPRO)/libnx/switch_rules

BUILD		:=	build
# Each launcher's own source/ plus the shared bootstrap/source (+ vendor).
SOURCES		+=	source $(COMMON_DIR)/source $(COMMON_DIR)/source/vendor
INCLUDES	+=	source $(COMMON_DIR)/source

#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(ARCH) $(DEFINES) \
			-DNXJS_RUNTIME_DEFAULT="\"$(NXJS_RUNTIME_DEFAULT)\""
CFLAGS	+=	$(INCLUDE) -D__SWITCH__ -D_DEFAULT_SOURCE

CXXFLAGS	:=	$(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	+=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	+=	-lnx
LIBDIRS	:=	$(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(if $(filter /%,$(dir)),$(dir),$(CURDIR)/$(dir)))

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

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(if $(filter /%,$(dir)),$(dir),$(CURDIR)/$(dir))) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

#---------------------------------------------------------------------------------
# NRO-flavor metadata (icon + nacp). NSP flavor sets CONFIG_JSON instead.
#---------------------------------------------------------------------------------
ifeq ($(NXJS_OUTPUT),nro)
ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.jpg)
	ifneq (,$(findstring icon.jpg,$(icons)))
		export APP_ICON := $(TOPDIR)/icon.jpg
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
endif

ifeq ($(NXJS_OUTPUT),nsp)
	export APP_JSON := $(TOPDIR)/$(CONFIG_JSON)
endif

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf $(TARGET).nso $(TARGET).npdm

#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

ifeq ($(NXJS_OUTPUT),nsp)
all	:	$(OUTPUT).nso $(OUTPUT).npdm
else
all	:	$(OUTPUT).nro
$(OUTPUT).nro	:	$(OUTPUT).elf $(OUTPUT).nacp
endif

$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	:

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
