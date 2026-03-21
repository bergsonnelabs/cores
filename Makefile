# Core Firmware SDK — Top-level Makefile
#
# Usage:
#   make                    # Build blink for Core.U.2 (default)
#   make TILE=Core-U-2-a PROJECT=blink
#   make flash              # Flash via USB DFU
#   make clean
#
# This Makefile is the Phase 1 build system. It will be supplemented
# (and eventually replaced) by CMake as the project grows.

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

# ---- Tile → MCU mapping ----
# TODO: This will be replaced by tilegen reading the JSON

ifeq ($(TILE),Core-U-1-a)
  MCU_FAMILY  = stm32l4xx
  MCU_PART    = STM32L422xx
  CPU         = cortex-m4
  FPU         = fpv4-sp-d16
  FLOAT_ABI   = hard
  LDSCRIPT    = link/stm32l422tb.ld
  STARTUP     = sdk/device/stm32l4xx/startup_stm32l422xx.s
else ifeq ($(TILE),Core-U-2-a)
  MCU_FAMILY  = stm32l4xx
  MCU_PART    = STM32L422xx
  CPU         = cortex-m4
  FPU         = fpv4-sp-d16
  FLOAT_ABI   = hard
  LDSCRIPT    = link/stm32l422tb.ld
  STARTUP     = sdk/device/stm32l4xx/startup_stm32l422xx.s
else
  $(error Unknown TILE: $(TILE). Supported: Core-U-1-a, Core-U-2-a)
endif

# ---- Paths ----

BUILD_DIR = build/$(TILE)/$(PROJECT)
TARGET    = $(BUILD_DIR)/$(PROJECT)

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
CFLAGS += -Og -g3

ASFLAGS = $(CPU_FLAGS) -Wall

LDFLAGS  = $(CPU_FLAGS)
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -specs=nosys.specs -specs=nano.specs
LDFLAGS += -Wl,-Map=$(TARGET).map,--cref

# ---- Object files ----

C_OBJS   = $(addprefix $(BUILD_DIR)/, $(C_SOURCES:.c=.o))
ASM_OBJS = $(addprefix $(BUILD_DIR)/, $(ASM_SOURCES:.s=.o))
OBJECTS  = $(C_OBJS) $(ASM_OBJS)

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

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "  AS    $<"
	@$(AS) $(ASFLAGS) -c $< -o $@

size: $(TARGET).elf
	@echo ""
	@$(SIZE) $<
	@echo ""

clean:
	rm -rf build/

flash: $(TARGET).bin
	@echo "  DFU   Flashing $(TARGET).bin via USB DFU..."
	dfu-util -a 0 -s 0x08000000:leave -D $<
