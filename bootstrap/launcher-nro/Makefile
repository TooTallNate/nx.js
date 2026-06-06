#---------------------------------------------------------------------------------
# nx.js slim bootstrap launcher (NRO) — a tiny homebrew NRO that chainloads the
# shared nx.js runtime from sdmc:/nx.js/ via envSetNextLoad. libnx only (no
# V8/Skia), so the output is a few hundred KB.
#
# The shared runtime-resolution + error UI live in ../source (compiled in via
# common.mk); this dir's source/ is just the NRO launch step.
#---------------------------------------------------------------------------------
# Absolute dir of THIS Makefile (so `include` works from the recursive build/
# sub-make where CURDIR changes).
THIS_MK_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

TARGET		:=	bootstrap
NXJS_OUTPUT	:=	nro

# Use the shared nx.js default icon (the same icon.jpg embedded in nxjs.nro at
# the repo root). ICON is relative to this launcher dir (rules prepend TOPDIR).
ICON		:=	../../icon.jpg

include $(THIS_MK_DIR)/../common.mk
