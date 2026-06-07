# Official name: FujiBlogger
# ProDOS filenames are limited to 15 characters (A-Z, 0-9, period only).
# "FUJIBLOGGER.SYSTEM" = 18 chars — exceeds the limit.
# The binary is therefore kept as "FUJIBLOG" → "FUJIBLOG.SYSTEM" (15 chars exactly).
PRODUCT = fujiblog
VERSION = 0.0.2
PLATFORMS += apple2

# You can run 'make <platform>' to build for a specific platform,
# or 'make <platform>/<target>' for a platform-specific target.
# Example shortcuts:
#   make coco        → build for coco
#   make apple2/disk → build the 'disk' target for apple2

# SRC_DIRS may use the literal %PLATFORM% token.
# It expands to the chosen PLATFORM plus any of its combos.
SRC_DIRS = src src/%PLATFORM%

# FUJINET_LIB can be
# - a version number such as 4.10.0
# - a directory which contains the libs for each platform
# - a zip file with an archived fujinet-lib
# - a URL to a git repo
# - empty which will use whatever is the latest
# - undefined, no fujinet-lib will be used
FUJINET_LIB = 4.10.0

# HIRESTXT_LIB can be
# - a version number such as 0.5.0.2
# - a directory which contains the built library
# - a URL to a git repo
# - empty which will use whatever is the latest
# - undefined, no hirestxt-mod will be used
# Only used for coco/dragon builds.
#HIRESTXT_LIB =

# Define extra dirs ("combos") that expand with a platform.
# Format: platform+=combo1,combo2
PLATFORM_COMBOS = \
  c64+=commodore \
  atarixe+=atari \
  msxrom+=msx \
  msxdos+=msx \
  adam_cpm+=adam

# Use a project-local linker config that raises __HIMEM__ from $9600 to $B000.
# The Apple IIc has RAM to $BEFF; the stock limit targets 48 KB machines.
LDFLAGS += -C apple2-fujiblog.cfg
CFLAGS  += -DVERSION=\"$(VERSION)\"

SPLASH_BIN = build/apple2/splash.a2s

include mekkogx/toplevel-rules.mk

# ── Splash build rules ────────────────────────────────────────────────────────

SPLASH_LOGO_HGR = splash/logo.hgr
SPLASH_ASM      = splash/splash_crt0.s splash/logo.s
SPLASH_CSRC     = splash/splash_main.c
CC65_LIB        = /usr/share/cc65/lib
VERSION_STAMP   = build/apple2/version.stamp

$(SPLASH_LOGO_HGR): splash/gen_hgr.py splash/fujiblogger-logo.png
	python3 $< splash/fujiblogger-logo.png $@

build/apple2:
	mkdir -p $@

# Version stamp: rewritten only when VERSION actually changes, so the splash
# (which bakes VERSION into splash_main.c via -DVERSION) is rebuilt on a bump.
.PHONY: force_version_stamp
$(VERSION_STAMP): force_version_stamp | build/apple2
	@printf '%s' '$(VERSION)' | cmp -s - $@ 2>/dev/null || printf '%s' '$(VERSION)' > $@

$(SPLASH_BIN): $(SPLASH_ASM) $(SPLASH_CSRC) $(SPLASH_LOGO_HGR) apple2-splash.cfg $(VERSION_STAMP) | build/apple2
	ca65 -t apple2 -o build/apple2/splash_crt0.o splash/splash_crt0.s
	ca65 -t apple2 -o build/apple2/logo.o       splash/logo.s
	cc65 -t apple2 -DVERSION=\"$(VERSION)\" -o build/apple2/splash_main.asm splash/splash_main.c
	ca65 -t apple2 -o build/apple2/splash_main.o build/apple2/splash_main.asm
	ld65 -C apple2-splash.cfg -o $@ \
	    build/apple2/splash_crt0.o \
	    build/apple2/logo.o \
	    build/apple2/splash_main.o \
	    $(CC65_LIB)/apple2.lib

# If you need to add extra platform-specific steps, do it below:
#   coco/r2r:: coco/custom-step1
#   coco/r2r:: coco/custom-step2
# or
#   apple2/disk: apple2/custom-step1 apple2/custom-step2
