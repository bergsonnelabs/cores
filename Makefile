# Core Firmware SDK — Top-level Makefile
#
# Usage (from SDK root, building an example):
#   make                          # Build blink for Core.ST.L4.2 (default)
#   make TILE=Core.ST.L4.2 PROJECT=blink
#   make flash                    # Flash via USB DFU
#   make generate                 # Run coregen only (no compile)
#   make clean
#
# Usage (from your own project folder):
#   cd my-firmware && make        # Uses per-project Makefile which calls back here
#
# Usage (external project, explicit):
#   make TILE=Core.ST.W5 PROJECT=my-firmware PROJECT_DIR=/path/to/my-firmware

# ---- SDK root (absolute path to this file's directory) ----

SDK_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# ---- Verbosity ----
# Default: quiet (shows GEN, LD, BIN/HEX, size). Use V=1 for full output.

V     ?= 0
ifeq ($(V),0)
  Q              := @
  LOG            := @:
  COREGEN_QUIET  := > /dev/null
else
  Q              :=
  LOG            := @echo
  COREGEN_QUIET  :=
endif

# ---- Configuration ----

TILE    ?= Core.ST.L4.2
PROJECT ?= blink

# ---- Public Core name aliases ----
# The vendor-segmented public names (Core.ST.<family>.<n>) resolve onto the
# canonical, DB-synced definition stems. Either form is accepted as TILE; the
# alias rewrites to the stem so the JSON lookup + MCU mapping below stay
# unchanged. Each public name maps to the most-complete definition for its MCU.
ifeq ($(TILE),Core.ST.L4.1)
  override TILE := Core-ST-L4-1-b
else ifeq ($(TILE),Core.ST.L4.2)
  override TILE := Core-ST-L4-2-a
else ifeq ($(TILE),Core.ST.H5.1)
  override TILE := Core-ST-H5-1-a
else ifeq ($(TILE),Core.ST.L0.1)
  override TILE := Core-ST-L0-1-a
else ifeq ($(TILE),Core.ST.W5)
  override TILE := Core-ST-W5-b
endif

# Project directory — override to point at any folder outside the SDK
PROJECT_DIR ?= $(SDK_DIR)examples/$(PROJECT)

# Toolchain
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
SIZE    = $(PREFIX)size
GDB     = $(PREFIX)gdb

# Coregen
COREGEN      = python3 $(SDK_DIR)tools/coregen/coregen.py
TILE_JSON    = $(SDK_DIR)definitions/$(TILE).json
CONFIG_JSON = $(PROJECT_DIR)/config.json
GEN_DIR      = $(PROJECT_DIR)/coregen

# ---- Tile → MCU mapping ----
# coregen generates the headers; the Makefile still needs to know
# CPU architecture and linker script for compiler flags.

ifeq ($(TILE),$(filter $(TILE),Core-ST-L4-1-a Core-ST-L4-1-b Core-ST-L4-2-a))
  MCU_FAMILY  = stm32l4xx
  MCU_PART    = STM32L422xx
  CPU         = cortex-m4
  FPU         = fpv4-sp-d16
  FLOAT_ABI   = hard
  LDSCRIPT    = $(SDK_DIR)sdk/device/stm32l422tb.ld
  STARTUP     = $(SDK_DIR)sdk/device/stm32l4xx/startup_stm32l422xx.s
  OPENOCD_CFG = $(SDK_DIR)sdk/debug/stm32l4.cfg
else ifeq ($(TILE),Core-ST-L0-1-a)
  MCU_FAMILY  = stm32l0xx
  MCU_PART    = STM32L011xx
  CPU         = cortex-m0plus
  LDSCRIPT    = $(SDK_DIR)sdk/device/stm32l011e4.ld
  STARTUP     = $(SDK_DIR)sdk/device/stm32l0xx/startup_stm32l011xx.s
  OPENOCD_CFG = $(SDK_DIR)sdk/debug/stm32l0.cfg
else ifeq ($(TILE),Core-ST-W5-b)
  MCU_FAMILY  = stm32wbaxx
  MCU_PART    = STM32WBA55xx
  CPU         = cortex-m33
  FPU         = fpv5-sp-d16
  FLOAT_ABI   = hard
  LDSCRIPT    = $(SDK_DIR)sdk/device/stm32wba55hg.ld
  STARTUP     = $(SDK_DIR)sdk/device/stm32wbaxx/startup_stm32wba55xx.s
  OPENOCD_CFG = $(SDK_DIR)sdk/debug/stm32wba.cfg
  FLASH_TOOL  = cubeprog
else ifeq ($(TILE),Core-ST-H5-1-a)
  MCU_FAMILY  = stm32h5xx
  MCU_PART    = STM32H523xx
  CPU         = cortex-m33
  FPU         = fpv5-sp-d16
  FLOAT_ABI   = hard
  LDSCRIPT    = $(SDK_DIR)sdk/device/stm32h523he.ld
  STARTUP     = $(SDK_DIR)sdk/device/stm32h5xx/startup_stm32h523xx.s
  OPENOCD_CFG = $(SDK_DIR)sdk/debug/stm32h5.cfg
else
  $(error Unknown TILE: $(TILE). Supported: Core-ST-L0-1-a, Core-ST-L4-1-a, Core-ST-L4-1-b, Core-ST-L4-2-a, Core-ST-W5-b, Core-ST-H5-1-a — or public names Core.ST.L0.1 / Core.ST.L4.1 / Core.ST.L4.2 / Core.ST.W5 / Core.ST.H5.1)
endif

# ---- Bootloader support ----
# Bootloader mode is read from config.json: "bootloader": "custom"|"rom"|"none"
#
#   "custom" — Custom DFU 1.1 bootloader at 0x08000000, app at 0x08002000.
#   "rom"    — ROM DfuSe bootloader (0483:DF11), app at 0x08000000.
#   "none"   — No DFU support. Flash via SWD or BOOT0.
#
# The mode maps to BOOTLOADER/ROM_DFU flags below. Command-line overrides
# still work (e.g. make BOOTLOADER=1) because ?= defers to explicit values.

_BOOT_MODE := $(shell python3 -c "import json; print(json.load(open('$(CONFIG_JSON)')).get('bootloader','none'))" 2>/dev/null || echo none)

ifeq ($(_BOOT_MODE),custom)
  BOOTLOADER ?= 1
  ROM_DFU    ?= 0
else ifeq ($(_BOOT_MODE),rom)
  BOOTLOADER ?= 0
  ROM_DFU    ?= 1
else
  BOOTLOADER ?= 0
  ROM_DFU    ?= 0
endif

ifeq ($(BOOTLOADER),1)
  ifeq ($(ROM_DFU),1)
    $(error BOOTLOADER=1 and ROM_DFU=1 are mutually exclusive)
  endif
  APP_ADDR   = 0x08002000
  APP_OFFSET = $(APP_ADDR)UL
  ifeq ($(TILE),$(filter $(TILE),Core-ST-L4-1-a Core-ST-L4-1-b Core-ST-L4-2-a))
    LDSCRIPT = $(SDK_DIR)sdk/device/stm32l422tb_app.ld
  endif
  ifeq ($(TILE),Core-ST-H5-1-a)
    LDSCRIPT = $(SDK_DIR)sdk/device/stm32h523he_app.ld
  endif
endif

ifeq ($(ROM_DFU),1)
  ifeq ($(TILE),$(filter $(TILE),Core-ST-L4-1-a Core-ST-L4-1-b Core-ST-L4-2-a))
    LDSCRIPT = $(SDK_DIR)sdk/device/stm32l422tb_romdfu.ld
  endif
  # Core.ST.H5: standard linker script already has noinit reservation and
  # app at 0x08000000 — no separate ROM DFU linker script needed.
endif

# ---- Paths ----

BUILD_DIR = $(PROJECT_DIR)/build
TARGET    = $(BUILD_DIR)/$(PROJECT)

# STM32CubeProgrammer CLI — used for tiles where OpenOCD lacks support (Core.ST.W5 / WBA55).
# Override on the command line if installed elsewhere.
STM32_PROG_CLI ?= /Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI

# Default flash tool is openocd; Core.ST.W5 overrides to cubeprog above.
FLASH_TOOL ?= openocd

# ---- Extract SYSCLK for SWO baud rate calc ----
SYSCLK_MHZ = $(shell python3 -c "\
import json; \
level = json.load(open('$(CONFIG_JSON)'))['clock']; \
configs = json.load(open('$(TILE_JSON)')).get('clock',{}).get('configurations',[]); \
print(next((c['sysclk_mhz'] for c in configs if c['name']==level), 80))" 2>/dev/null || echo 80)
SYSCLK_HZ  = $(shell echo $$(($(SYSCLK_MHZ) * 1000000)))

# ---- Sources ----

C_SOURCES   = $(PROJECT_DIR)/main.c
ASM_SOURCES = $(STARTUP)

# ---- Compiler flags ----

CPU_FLAGS = -mcpu=$(CPU) -mthumb
ifdef FPU
  CPU_FLAGS += -mfpu=$(FPU) -mfloat-abi=$(FLOAT_ABI)
endif

CFLAGS  = $(CPU_FLAGS)
CFLAGS += -Wall -Wextra -Wshadow -Wdouble-promotion
CFLAGS += -fdata-sections -ffunction-sections -fno-common
CFLAGS += -std=c17
CFLAGS += -D$(MCU_PART)
CFLAGS += -I$(GEN_DIR)
CFLAGS += -I$(PROJECT_DIR)
CFLAGS += -I$(SDK_DIR)sdk/ll
CFLAGS += -I$(SDK_DIR)sdk/hal
CFLAGS += -I$(SDK_DIR)sdk/tal
CFLAGS += -I$(SDK_DIR)sdk/core
CFLAGS += -Og -g3

ifdef APP_OFFSET
  CFLAGS += -DAPP_OFFSET=$(APP_OFFSET)
endif
ifeq ($(ROM_DFU),1)
  CFLAGS += -DROM_DFU
endif

# ---- Tile driver support (optional) ----
TILES_ENABLED ?= 0
ifeq ($(TILES_ENABLED),1)
  -include $(GEN_DIR)/core_drivers.mk
  CFLAGS += -I"$(SDK_DIR)" -I"$(SDK_DIR)drivers" -I"$(SDK_DIR)hal"
  TILES_SOURCES =
  ifdef TILES_DRIVERS
    TILES_SOURCES += $(foreach drv,$(TILES_DRIVERS),$(SDK_DIR)drivers/$(drv).c)
  endif
endif

# ---- WAMR runtime support (optional; A4c) ----
# Opt-in via `WAMR_ENABLED=1`. When enabled, links the WAMR
# interpreter into the firmware so a user's DSL-compiled `.wasm`
# blob can execute on-device. Gated only to Cortex-M33 targets
# (Core.ST.H5, Core.ST.W5) — Core.ST.L4's M4 footprint is over budget per the
# A4b spike and routes through a separate interpreter path.
WAMR_ENABLED ?= 0
ifeq ($(WAMR_ENABLED),1)
  ifneq ($(CPU),cortex-m33)
    $(error WAMR_ENABLED=1 is only supported on Cortex-M33 Cores (Core.ST.H5.1, Core.ST.W5))
  endif
  include $(SDK_DIR)sdk/wamr/wamr.mk
endif

# ---- BLE support (Core.ST.W5 only) ----
BLE_ENABLED ?= 0
ifeq ($(BLE_ENABLED),1)
  ifeq ($(MCU_PART),STM32WBA55xx)
    LDSCRIPT = $(SDK_DIR)sdk/device/stm32wba55hg_ble.ld
    CFLAGS += -I$(SDK_DIR)sdk/ble/include
    CFLAGS += -I$(SDK_DIR)sdk/ble/include/auto
    CFLAGS += -I$(SDK_DIR)sdk/ble/link_layer/inc
    CFLAGS += -DBLE_ENABLED=1 -DBASIC_FEATURES=1 -DBLE=1
    CFLAGS += -D'__PACKED_STRUCT=struct __attribute__((packed))'
    CFLAGS += -D'__PACKED_UNION=union __attribute__((packed))'
    CFLAGS += -include $(SDK_DIR)sdk/ble/include/cmsis_compiler.h
    BLE_LIBS  = $(SDK_DIR)sdk/ble/lib/stm32wba_ble_stack_basic.a
    BLE_LIBS += $(SDK_DIR)sdk/ble/lib/LinkLayer_BLE_Basic_lib.a
    BLE_SOURCES = $(wildcard $(SDK_DIR)sdk/ble/*.c)
    BLE_OBJS = $(addprefix $(BUILD_DIR)/sdk/ble/, $(notdir $(BLE_SOURCES:.c=.o)))
    BLE_OBJS += $(BUILD_DIR)/sdk/core/core_ble.o
  else
    $(error BLE_ENABLED=1 is only supported for Core.ST.W5 (STM32WBA55xx))
  endif
endif

ASFLAGS = $(CPU_FLAGS) -Wall

LDFLAGS  = $(CPU_FLAGS)
LDFLAGS += -T$(LDSCRIPT)
ifeq ($(BLE_ENABLED),1)
LDFLAGS += -Wl,--no-gc-sections
else
LDFLAGS += -Wl,--gc-sections
endif
LDFLAGS += -specs=nosys.specs -specs=nano.specs
LDFLAGS += -Wl,-Map=$(TARGET).map,--cref

# ---- Object files ----

C_OBJS   = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SOURCES:.c=.o)))
ASM_OBJS = $(patsubst $(SDK_DIR)%.s, $(BUILD_DIR)/%.o, $(ASM_SOURCES))
HAL_SOURCES = $(wildcard $(SDK_DIR)sdk/hal/*.c)
HAL_OBJS = $(addprefix $(BUILD_DIR)/sdk/hal/, $(notdir $(HAL_SOURCES:.c=.o)))
GEN_OBJS = $(GEN_SOURCES:.c=.o)
OBJECTS  = $(C_OBJS) $(ASM_OBJS) $(HAL_OBJS) $(GEN_OBJS)

ifeq ($(TILES_ENABLED),1)
  TILES_OBJS = $(addprefix $(BUILD_DIR)/tiles/, $(notdir $(TILES_SOURCES:.c=.o)))
  OBJECTS += $(TILES_OBJS)
endif

ifeq ($(BLE_ENABLED),1)
  OBJECTS += $(BLE_OBJS)
endif

ifeq ($(WAMR_ENABLED),1)
  OBJECTS += $(WAMR_ALL_OBJS)
endif

# ---- Default goal ----

.DEFAULT_GOAL := all

# ---- Generated headers (coregen) ----

GEN_HEADERS = $(GEN_DIR)/core_pads.h $(GEN_DIR)/core_board.h $(GEN_DIR)/core_interfaces.h

ifneq ($(wildcard $(CONFIG_JSON)),)
  GEN_HEADERS  += $(GEN_DIR)/core_config.h $(GEN_DIR)/core_init.h
  GEN_SOURCES   = $(GEN_DIR)/core_init.c
  COREGEN_FLAGS = --config $(CONFIG_JSON)
  # Per-project WAMR natives ride alongside core_init.c so the adapters
  # for pad/pwm/adc/dac (which reach into PAD_*_PORT macros, core_dac,
  # core_adc1, etc.) compile in the same translation-unit scope. Only
  # link when the project pulled WAMR in — otherwise the symbols are
  # generated but ignored at link time.
  ifeq ($(WAMR_ENABLED),1)
    GEN_HEADERS += $(GEN_DIR)/studio_natives_project.h
    GEN_SOURCES += $(GEN_DIR)/studio_natives_project.c
  endif
else
  GEN_SOURCES   =
  COREGEN_FLAGS =
endif

# core_drivers.mk is an included makefile — it must have its own recipe so
# GNU Make detects it was remade and restarts (picking up TILES_DRIVERS).
$(GEN_DIR)/core_drivers.mk: $(TILE_JSON) $(wildcard $(CONFIG_JSON)) $(SDK_DIR)tools/coregen/coregen.py $(SDK_DIR)tools/coregen/templates/*.j2
	@mkdir -p $(GEN_DIR)
	@echo "  GEN   $(TILE)"
	$(Q)$(COREGEN) $(TILE_JSON) $(GEN_DIR) $(COREGEN_FLAGS) $(COREGEN_QUIET)

# Stamp prevents re-running coregen for each header file target.
GEN_STAMP = $(GEN_DIR)/.coregen.stamp
$(GEN_STAMP): $(GEN_DIR)/core_drivers.mk
	$(Q)touch $(GEN_STAMP)

$(GEN_HEADERS): $(GEN_STAMP)

.PHONY: generate
generate: $(GEN_STAMP)

# ---- Rules ----

.PHONY: all clean distclean flash size

all: $(TARGET).bin $(TARGET).hex size

$(TARGET).elf: $(OBJECTS) $(LDSCRIPT)
	@echo "  LD    $(notdir $@)"
ifeq ($(BLE_ENABLED),1)
	$(Q)$(CC) $(OBJECTS) $(BLE_LIBS) $(LDFLAGS) -o $@
else
	$(Q)$(CC) $(OBJECTS) $(LDFLAGS) -o $@
endif

$(TARGET).bin: $(TARGET).elf
	@echo "  BIN   $(notdir $@)"
	$(Q)$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	@echo "  HEX   $(notdir $@)"
	$(Q)$(OBJCOPY) -O ihex $< $@

# C sources depend on generated headers
$(BUILD_DIR)/%.o: $(PROJECT_DIR)/%.c $(GEN_HEADERS)
	$(Q)mkdir -p $(dir $@)
	$(LOG) "  CC    $<"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

# Generated C sources (core_init.c, studio_natives_project.c, etc.).
# Static pattern: each `<name>.o` in GEN_OBJS pairs with the matching
# `<name>.c` in GEN_SOURCES. Static-pattern syntax keeps the rule
# crisp even when GEN_DIR is an absolute path (regular `%` pattern
# rules silently fail to match in that case on some make builds).
$(GEN_OBJS): $(GEN_DIR)/%.o: $(GEN_DIR)/%.c $(GEN_HEADERS)
	$(LOG) "  CC    $<"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

# HAL sources
$(BUILD_DIR)/sdk/hal/%.o: $(SDK_DIR)sdk/hal/%.c $(GEN_HEADERS)
	$(Q)mkdir -p $(dir $@)
	$(LOG) "  CC    $<"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sdk/device/%.o: $(SDK_DIR)sdk/device/%.s
	$(Q)mkdir -p $(dir $@)
	$(LOG) "  AS    $(notdir $<)"
	$(Q)$(AS) $(ASFLAGS) -c $< -o $@

# Tile driver sources
ifeq ($(TILES_ENABLED),1)
$(BUILD_DIR)/tiles/%.o: $(SDK_DIR)hal/%.c $(GEN_HEADERS)
	$(Q)mkdir -p $(dir $@)
	$(LOG) "  CC    $(notdir $<)"
	$(Q)$(CC) $(CFLAGS) -c "$<" -o $@

$(BUILD_DIR)/tiles/%.o: $(SDK_DIR)drivers/%.c $(GEN_HEADERS)
	$(Q)mkdir -p $(dir $@)
	$(LOG) "  CC    $(notdir $<)"
	$(Q)$(CC) $(CFLAGS) -c "$<" -o $@
endif

# BLE sources (relaxed warnings — vendor headers are noisy)
ifeq ($(BLE_ENABLED),1)
$(BUILD_DIR)/sdk/ble/%.o: $(SDK_DIR)sdk/ble/%.c $(GEN_HEADERS)
	$(Q)mkdir -p $(dir $@)
	$(LOG) "  CC    $(notdir $<)"
	$(Q)$(CC) $(CFLAGS) -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -c $< -o $@

# core_ble.c — in sdk/core/ but only compiled for BLE builds
$(BUILD_DIR)/sdk/core/core_ble.o: $(SDK_DIR)sdk/core/core_ble.c $(GEN_HEADERS)
	$(Q)mkdir -p $(dir $@)
	$(LOG) "  CC    $(notdir $<)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
endif

size: $(TARGET).elf
	@echo ""
	$(Q)$(SIZE) $<
	@echo ""

clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -rf $(GEN_DIR)
	rm -f $(PROJECT_DIR)/core.h
	rm -f $(PROJECT_DIR)/tiles.h

# ---- Flash via OpenOCD (ST-Link) ----

flash: $(TARGET).elf
ifeq ($(FLASH_TOOL),cubeprog)
	$(STM32_PROG_CLI) -c port=SWD mode=UR -w $< -v -rst
else
	openocd -f $(OPENOCD_CFG) \
		-c "init" \
		-c "reset halt" \
		-c "program $< verify" \
		-c "reset run" \
		-c "exit"
endif

# ---- Flash via USB DFU ----
#
# flash-dfu:  Smart flash via USB DFU. Behavior depends on boot mode:
#
#   BOOTLOADER=1: 1200-baud touch triggers custom DFU bootloader.
#     Tries plain DFU 1.1 first, falls back to DfuSe at APP_ADDR.
#
#   ROM_DFU=1: 1200-baud touch triggers ROM bootloader (DfuSe).
#     Flashes at 0x08000000 with :leave to auto-reset into app.
#
#   Neither: Direct DfuSe flash to 0x08000000 (board must already
#     be in DFU mode, e.g. BOOT0 held high).
#
# flash-rom:  Always flashes at 0x08000000 via ROM DFU (for fresh boards
#             or when BOOT0 is held high). Works regardless of boot mode.

flash-dfu: $(TARGET).bin
ifeq ($(BOOTLOADER),1)
	@TTY=$$(ls /dev/tty.usbmodem* 2>/dev/null | head -1); \
	if [ -n "$$TTY" ]; then \
		echo "  DFU   Triggering reboot via 1200-baud touch on $$TTY..."; \
		stty -f "$$TTY" 1200 hupcl; \
		echo "  DFU   Waiting for DFU device..."; \
		sleep 4; \
	fi
	@_dfu_log=$$(mktemp); \
	_dfu_ok=0; \
	dfu-util -a 0 -R -D $< > "$$_dfu_log" 2>&1; \
	if grep -q "Download done" "$$_dfu_log"; then _dfu_ok=1; fi; \
	if [ "$$_dfu_ok" = "0" ]; then \
		dfu-util -a 0 -s $(APP_ADDR):leave -D $< > "$$_dfu_log" 2>&1; \
		if grep -q "Download done" "$$_dfu_log"; then _dfu_ok=1; fi; \
	fi; \
	if [ "$$_dfu_ok" = "1" ]; then \
		_sz=$$(grep -o '[0-9]* bytes$$' "$$_dfu_log" | tail -1); \
		echo "  DFU   Downloaded $${_sz:-firmware}"; \
		echo "  DFU   OK"; \
		echo "  DFU   Resetting..."; \
	else \
		cat "$$_dfu_log"; \
		echo "  DFU   FAILED"; \
	fi; \
	rm -f "$$_dfu_log"
else ifeq ($(ROM_DFU),1)
	@TTY=$$(ls /dev/tty.usbmodem* 2>/dev/null | head -1); \
	if [ -n "$$TTY" ]; then \
		echo "  DFU   Triggering reboot via 1200-baud touch on $$TTY..."; \
		stty -f "$$TTY" 1200 hupcl; \
		echo "  DFU   Waiting for ROM DFU device (0483:df11)..."; \
		sleep 4; \
	fi
	@_dfu_log=$$(mktemp); \
	dfu-util -a 0 -s 0x08000000:leave -D $< > "$$_dfu_log" 2>&1 || true; \
	if grep -q "File downloaded successfully" "$$_dfu_log"; then \
		_sz=$$(grep -o 'size = [0-9]*' "$$_dfu_log" | tail -1 | grep -o '[0-9]*'); \
		echo "  DFU   Downloaded $${_sz:-?} bytes"; \
		echo "  DFU   Resetting..."; \
		sleep 5; \
		if ls /dev/tty.usbmodem* >/dev/null 2>&1; then \
			echo "  DFU   OK"; \
		else \
			echo "  DFU   OK — power cycle if the app hasn't started"; \
		fi; \
	else \
		cat "$$_dfu_log"; \
		echo "  DFU   FAILED"; \
		rm -f "$$_dfu_log"; \
		exit 1; \
	fi; \
	rm -f "$$_dfu_log"
else
	dfu-util -a 0 -s 0x08000000:leave -D $<
endif

flash-rom: $(TARGET).bin
	dfu-util -a 0 -s 0x08000000:leave -D $<

# ---- Flash the DFU bootloader itself ----

flash-bootloader:
	$(MAKE) -C $(SDK_DIR)sdk/bootloader flash

# ---- GDB debug session ----
# Step 1: Run 'make openocd' in one terminal (starts GDB server)
# Step 2: Run 'make gdb' in another terminal (connects debugger)

openocd: $(TARGET).elf
	openocd -f $(OPENOCD_CFG) \
		-c "init" \
		-c "reset halt" \
		-c "program $< verify" \
		-c "reset halt"

gdb: $(TARGET).elf
	$(GDB) $< \
		-ex "target extended-remote :3333" \
		-ex "monitor reset halt" \
		-ex "load" \
		-ex "break main" \
		-ex "continue"

# ---- SWO trace output ----

swo: $(TARGET).elf
	openocd -f $(OPENOCD_CFG) \
		-c "init" \
		-c "reset halt" \
		-c "program $< verify" \
		-c "tpiu config internal /dev/stdout uart off $(SYSCLK_HZ) 2000000" \
		-c "itm port 0 on" \
		-c "reset run"

# ---- End-to-end validation ----
.PHONY: validate validate-generate
validate:
	@python3 $(SDK_DIR)tools/validate.py

validate-generate:
	@python3 $(SDK_DIR)tools/validate.py --generate-only
