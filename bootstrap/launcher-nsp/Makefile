#---------------------------------------------------------------------------------
# nx.js slim bootstrap forwarder (NSP) — a patched nx-hbloader that runs as the
# `main` NSO of an installable title. It resolves + loads the shared nx.js
# runtime NRO from sdmc:/nx.js/ (homebrew ABI), passing argv "nsp:" so the
# runtime mounts THIS title's RomFS (the app's main.js + assets).
#
# Builds an exefs NSO (`hbl.nso`) + NPDM (`hbl.npdm`) — consumed by @nx.js/nsp
# in --slim mode. Shared runtime-resolution + error UI come from ../source via
# common.mk; this dir's source/ is the (patched hbloader) launch step.
#
# Based on switchbrew/nx-hbloader + Skywalker25/Forwarder-Mod (see README).
#---------------------------------------------------------------------------------

# Absolute dir of THIS Makefile (so `include` works from the recursive build/
# sub-make where CURDIR changes).
THIS_MK_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

TARGET		:=	hbl
NXJS_OUTPUT	:=	nsp
CONFIG_JSON	:=	hbl.json

# The forwarder stubs exit() via __wrap_exit (it must never actually exit the
# process the normal way — see source/main.c).
LDFLAGS		:=	-Wl,-wrap,exit

include $(THIS_MK_DIR)/../common.mk
