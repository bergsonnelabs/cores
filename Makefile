# Core Firmware SDK — Top-level Makefile
#
# Usage:
#   make                    # Build blink for Core.U.2 (default)
#   make TILE=Core-U-2-a PROJECT=blink
#   make flash              # Flash via USB DFU
#   make generate           # Run coregen only (no compile)
#   make clean

# ---- Configuration ----

TILE    ?= Core-U-2-a
PROJECT ?= blink

# Toolchain
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
SIZE    = $(PREFIX)size
GDB     = $(PREFIX)gdb

# Coregen
COREGEN = python3 tools/coregen/coregen.py
TILE_JSON = tiles/$(TILE).json
PROJECT_JSON = projects/$(PROJECT)/project.json
GEN_DIR = build/generated/$(TILE)

# ---- Tile → MCU mapping ----
# coregen generates the headers; the Makefile still needs to know
# CPU architecture and linker script for compiler flags.

ifeq ($(TILE),$(filter $(TILE),Core-U-1-a Core-U-2-a))
  MCU_FAMILY  = stm32l4xx
  MCU_PART    = STM32L422xx
  CPU         = cortex-m4
  FPU         = fpv4-sp-d16
  FLOAT_ABI   = hard
  LDSCRIPT    = link/stm32l422tb.ld
  STARTUP     = sdk/device/stm32l4xx/startup_stm32l422xx.s
  OPENOCD_CFG = debug/stm32l4.cfg
else ifeq ($(TILE),Core-L-1-a)
  MCU_FAMILY  = stm32l0xx
  MCU_PART    = STM32L011xx
  CPU         = cortex-m0plus
  LDSCRIPT    = link/stm32l011e4.ld
  STARTUP     = sdk/device/stm32l0xx/startup_stm32l011xx.s
  OPENOCD_CFG = debug/stm32l0.cfg
else ifeq ($(TILE),Core-W-b)
  MCU_FAMILY  = stm32wbaxx
  MCU_PART    = STM32WBA55xx
  CPU         = cortex-m33
  FPU         = fpv5-sp-d16
  FLOAT_ABI   = hard
  LDSCRIPT    = link/stm32wba55hg.ld
  STARTUP     = sdk/device/stm32wbaxx/startup_stm32wba55xx.s
  OPENOCD_CFG = debug/stm32l4.cfg
else ifeq ($(TILE),Core-H-1-a)
  MCU_FAMILY  = stm32h5xx
  MCU_PART    = STM32H523xx
  CPU         = cortex-m33
  FPU         = fpv5-sp-d16
  FLOAT_ABI   = hard
  LDSCRIPT    = link/stm32h523he.ld
  STARTUP     = sdk/device/stm32h5xx/startup_stm32h523xx.s
  OPENOCD_CFG = debug/stm32l4.cfg
else
  $(error Unknown TILE: $(TILE). Supported: Core-L-1-a, Core-U-1-a, Core-U-2-a, Core-W-b, Core-H-1-a)
endif

# ---- Paths ----

BUILD_DIR = build/$(TILE)/$(PROJECT)
TARGET    = $(BUILD_DIR)/$(PROJECT)

# ---- Extract SYSCLK for SWO baud rate calc ----
SYSCLK_MHZ = $(shell python3 -c "import json; print(json.load(open('$(PROJECT_JSON)'))['clock']['sysclk_mhz'])" 2>/dev/null || echo 80)
SYSCLK_HZ  = $(shell echo $$(($(SYSCLK_MHZ) * 1000000)))

# ---- Sources ----

C_SOURCES  = projects/$(PROJECT)/main.c
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
CFLAGS += -Isdk/ll
CFLAGS += -Isdk/hal
CFLAGS += -Og -g3

# ---- Kiln driver support (optional) ----
KILN_DIR ?= Kiln
KILN_ENABLED ?= 0
ifeq ($(KILN_ENABLED),1)
  CFLAGS += -I"$(KILN_DIR)/inc" -I"$(KILN_DIR)/HAL" -I"$(KILN_DIR)/Templates"
  KILN_SOURCES = $(KILN_DIR)/HAL/tiles_hal_core.c
  # Add specific driver sources as needed via KILN_DRIVERS
  ifdef KILN_DRIVERS
    KILN_SOURCES += $(foreach drv,$(KILN_DRIVERS),$(KILN_DIR)/Src/$(drv).c)
  endif
endif

ASFLAGS = $(CPU_FLAGS) -Wall

LDFLAGS  = $(CPU_FLAGS)
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -specs=nosys.specs -specs=nano.specs
LDFLAGS += -Wl,-Map=$(TARGET).map,--cref

# ---- Object files ----

C_OBJS   = $(addprefix $(BUILD_DIR)/, $(C_SOURCES:.c=.o))
ASM_OBJS = $(addprefix $(BUILD_DIR)/, $(ASM_SOURCES:.s=.o))
GEN_OBJS = $(GEN_SOURCES:.c=.o)
HAL_SOURCES = $(wildcard sdk/hal/*.c)
HAL_OBJS = $(addprefix $(BUILD_DIR)/, $(HAL_SOURCES:.c=.o))
OBJECTS  = $(C_OBJS) $(ASM_OBJS) $(GEN_OBJS) $(HAL_OBJS)

# Kiln driver objects (when KILN_ENABLED=1)
ifeq ($(KILN_ENABLED),1)
  KILN_OBJS = $(addprefix $(BUILD_DIR)/kiln/, $(notdir $(KILN_SOURCES:.c=.o)))
  OBJECTS += $(KILN_OBJS)
endif

# ---- Default goal ----

.DEFAULT_GOAL := all

# ---- Generated headers (coregen) ----

GEN_HEADERS = $(GEN_DIR)/tile_pins.h $(GEN_DIR)/tile_board.h $(GEN_DIR)/tile_interfaces.h

# Add project-specific generated files if a project.json exists
ifneq ($(wildcard $(PROJECT_JSON)),)
  GEN_HEADERS += $(GEN_DIR)/tile_config.h $(GEN_DIR)/tile_init.h
  GEN_SOURCES = $(GEN_DIR)/tile_init.c
  COREGEN_FLAGS = --project $(PROJECT_JSON)
else
  GEN_SOURCES =
  COREGEN_FLAGS =
endif

$(GEN_HEADERS): $(TILE_JSON) $(wildcard $(PROJECT_JSON)) tools/coregen/coregen.py tools/coregen/templates/*.j2
	@echo "  GEN   $(TILE)"
	@$(COREGEN) $(TILE_JSON) $(GEN_DIR) $(COREGEN_FLAGS)

.PHONY: generate
generate: $(GEN_HEADERS)

# ---- Rules ----

.PHONY: all clean flash size

all: $(TARGET).bin $(TARGET).hex size

$(TARGET).elf: $(OBJECTS) $(LDSCRIPT)
	@echo "  LD    $@"
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	@echo "  BIN   $@"
	@$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	@echo "  HEX   $@"
	@$(OBJCOPY) -O ihex $< $@

# C sources depend on generated headers
$(BUILD_DIR)/%.o: %.c $(GEN_HEADERS)
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Generated C sources (tile_init.c etc.)
# These are produced by coregen alongside the headers
$(GEN_DIR)/tile_init.o: $(GEN_DIR)/tile_init.c $(GEN_HEADERS)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "  AS    $<"
	@$(AS) $(ASFLAGS) -c $< -o $@

# Kiln driver sources (compiled from external directory)
ifeq ($(KILN_ENABLED),1)
$(BUILD_DIR)/kiln/%.o: $(KILN_DIR)/HAL/%.c $(GEN_HEADERS)
	@mkdir -p $(dir $@)
	@echo "  CC    $(notdir $<)"
	@$(CC) $(CFLAGS) -c "$<" -o $@

$(BUILD_DIR)/kiln/%.o: $(KILN_DIR)/Src/%.c $(GEN_HEADERS)
	@mkdir -p $(dir $@)
	@echo "  CC    $(notdir $<)"
	@$(CC) $(CFLAGS) -c "$<" -o $@
endif

size: $(TARGET).elf
	@echo ""
	@$(SIZE) $<
	@echo ""

clean:
	rm -rf build/

# ---- Flash via OpenOCD (ST-Link) ----

flash: $(TARGET).elf
	openocd -f $(OPENOCD_CFG) \
		-c "init" \
		-c "reset halt" \
		-c "program $< verify" \
		-c "reset run" \
		-c "exit"

# ---- Flash via USB DFU (Core.U/H with USB bootloader) ----

flash-dfu: $(TARGET).bin
	dfu-util -a 0 -s 0x08000000:leave -D $<

# ---- GDB debug session ----
# Step 1: Run 'make openocd' in one terminal (starts GDB server)
# Step 2: Run 'make gdb' in another terminal (connects debugger)
#
# In GDB: Ctrl-C to halt, 'c' to continue, 'b main' to set breakpoint,
# 'n' to step over, 's' to step into, 'p variable' to print, 'quit' to exit.

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
# Flashes firmware, enables SWO capture, prints ITM output.
# Press Ctrl-C to stop.

swo: $(TARGET).elf
	openocd -f $(OPENOCD_CFG) \
		-c "init" \
		-c "reset halt" \
		-c "program $< verify" \
		-c "tpiu config internal /dev/stdout uart off $(SYSCLK_HZ) 2000000" \
		-c "itm port 0 on" \
		-c "reset run"
