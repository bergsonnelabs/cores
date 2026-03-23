#!/bin/bash
set -e

BUILD_DIR="hybrid_builds_r3"
BASELINE_OBJS="$BUILD_DIR/baseline_objs.txt"

# Get baseline object list
if [ ! -f "$BASELINE_OBJS" ]; then
    echo "Need baseline_objs.txt"
    exit 1
fi

CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
SIZE=arm-none-eabi-size

MCU_FLAGS="-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb"

# Common flags from the working build
PROJ_DIR="/Users/jonathanfiene/Documents/STM32/Core-W Bluetooth"
REPO_DIR="/Users/jonathanfiene/STM32Cube/Repository/STM32Cube_FW_WBA_V1.6.1"
LDSCRIPT="${PROJ_DIR}/STM32WBA55HGFX_FLASH.ld"

LIBS="-l:LinkLayer_BLE_Basic_lib.a -l:stm32wba_ble_stack_basic.a"
LIB_DIRS="-L${REPO_DIR}/Middlewares/ST/STM32_WPAN/ble/stack/lib -L${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/lib"

# Test: swap ONLY ble_irq.o (needs externs from linklayer_plat which stays as working version)
echo "=== Test: ble_irq only ==="
OBJ_LIST=$(cat "$BASELINE_OBJS" | grep -v "stm32wbaxx_it.o" | tr '\n' ' ')
# Our ble_irq.o provides RADIO_IRQHandler and HASH_IRQHandler
# The working project's stm32wbaxx_it.o also provides these, so we remove it
# But stm32wbaxx_it.o also provides other handlers (SysTick, HardFault, etc.)
# We need to keep those... this is tricky

# Actually, let's just check if our ble_irq.o has ONLY RADIO+HASH handlers
arm-none-eabi-nm /Users/jonathanfiene/Documents/local/source/cores/build/Core-W-b/ble-beacon/sdk/ble/ble_irq.o 2>/dev/null | grep " T "
