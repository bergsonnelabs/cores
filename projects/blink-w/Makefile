# Project Makefile — run make from inside the project folder
TILE         := Core-W-b

PROJECT      := $(notdir $(CURDIR))
ROOT         := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))../..)

.PHONY: all clean clean-all distclean flash size
all clean clean-all distclean flash size:
	$(MAKE) -C $(ROOT) TILE=$(TILE) PROJECT=$(PROJECT) KILN_ENABLED=$(KILN_ENABLED) $@
