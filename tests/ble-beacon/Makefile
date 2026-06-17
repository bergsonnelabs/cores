# Project Makefile — run make from inside the project folder
TILE         := Core.ST.W5.1
PROJECT      := $(notdir $(CURDIR))
PROJECT_DIR  := $(CURDIR)
ROOT         := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))../..)
BLE_ENABLED  ?= 1

.PHONY: all clean clean-all distclean flash flash-dfu size generate
all clean clean-all distclean flash flash-dfu size generate:
	$(MAKE) -C $(ROOT) TILE=$(TILE) PROJECT=$(PROJECT) PROJECT_DIR=$(PROJECT_DIR) BLE_ENABLED=$(BLE_ENABLED) TILES_ENABLED=$(TILES_ENABLED) $@
