TILE         := $(shell python3 -c "import json,sys; print(json.load(open('config.json'))['core'])" 2>/dev/null || echo Core.ST.L4.2)
TILES_ENABLED := 0
PROJECT      := $(notdir $(CURDIR))
PROJECT_DIR  := $(abspath $(CURDIR))
SDK_ROOT     := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../..)

.PHONY: all clean distclean flash flash-dfu size generate
all clean distclean flash flash-dfu size generate:
	$(MAKE) -C $(SDK_ROOT) TILE=$(TILE) PROJECT=$(PROJECT) PROJECT_DIR=$(PROJECT_DIR) TILES_ENABLED=$(TILES_ENABLED) BOOTLOADER=1 $@
