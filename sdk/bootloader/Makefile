# DFU Bootloader for Core.ST.L4 (STM32L422TB)
#
# Standalone build — no coregen, no config.json.
# Produces a minimal bootloader binary (<8KB) that checks RTC BKP0
# for the DFU magic and either enters USB DFU or jumps to the app.
#
# Usage:
#   make            # Build
#   make size       # Show size
#   make flash      # Flash via BOOT0 + ROM DFU (first time)
#   make clean      # Clean

# ---- Toolchain ----

PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# ---- Paths ----

SDK_ROOT  = $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../../)
BUILD_DIR = build
TARGET    = $(BUILD_DIR)/bootloader

# ---- Sources ----

SOURCES = main.c
STARTUP = $(SDK_ROOT)/sdk/device/stm32l4xx/startup_stm32l422xx.s
LDSCRIPT = bootloader.ld

# ---- Flags ----

CPU_FLAGS  = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

CFLAGS  = $(CPU_FLAGS)
CFLAGS += -DSTM32L422xx
CFLAGS += -I$(SDK_ROOT)/sdk/ll
CFLAGS += -I$(SDK_ROOT)/sdk/hal
CFLAGS += -Os -flto
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fno-common -fno-builtin
CFLAGS += -std=gnu11

ASFLAGS = $(CPU_FLAGS) -DSTM32L422xx

LDFLAGS  = $(CPU_FLAGS)
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -specs=nosys.specs -specs=nano.specs
LDFLAGS += -flto
LDFLAGS += -Wl,--print-memory-usage

# ---- Objects ----

OBJECTS  = $(BUILD_DIR)/main.o
OBJECTS += $(BUILD_DIR)/startup.o

# ---- Quiet mode (V=1 for verbose) ----

Q = $(if $(V),,@)

# ---- Rules ----

.PHONY: all clean size flash

all: $(TARGET).bin size

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	@echo "  CC    $(notdir $<)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/startup.o: $(STARTUP) | $(BUILD_DIR)
	@echo "  AS    $(notdir $<)"
	$(Q)$(AS) $(ASFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJECTS) $(LDSCRIPT)
	@echo "  LD    $(notdir $@)"
	$(Q)$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	@echo "  BIN   $(notdir $@)"
	$(Q)$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	@echo "  HEX   $(notdir $@)"
	$(Q)$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR):
	@mkdir -p $@

size: $(TARGET).elf
	@echo ""
	$(Q)$(SIZE) $<

# Flash bootloader via BOOT0 + ROM DFU.
# No :leave — stay in DFU so the app can be flashed next.
flash: $(TARGET).bin
	dfu-util -a 0 -s 0x08000000 -D $<

clean:
	rm -rf $(BUILD_DIR)
