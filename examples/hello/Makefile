TILE         := $(shell python3 -c "import json; print(json.load(open('project.json'))['core'])" 2>/dev/null || echo Core-U-2-a)
KILN_ENABLED := 0
PROJECT      := $(notdir $(CURDIR))
PROJECT_DIR  := $(abspath $(CURDIR))
SDK_ROOT     := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../..)

.PHONY: all clean distclean flash flash-dfu size generate
all clean distclean flash flash-dfu size generate:
	$(MAKE) -C $(SDK_ROOT) TILE=$(TILE) PROJECT=$(PROJECT) PROJECT_DIR=$(PROJECT_DIR) KILN_ENABLED=$(KILN_ENABLED) BOOTLOADER=1 $@
