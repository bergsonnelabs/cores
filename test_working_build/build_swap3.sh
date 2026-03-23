#!/bin/bash
##############################################################################
# Build script to compile the working CubeIDE "Core-W Bluetooth" project
# using our ARM toolchain (arm-none-eabi-gcc from PATH).
#
# Purpose: Determine if the BLE advertising issue is in source code vs build system.
# This compiles the WORKING project's own source files with our toolchain.
##############################################################################
set -e

# Toolchain
CC=arm-none-eabi-gcc
AS=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
SIZE=arm-none-eabi-size

# Project paths
PROJ_DIR="/Users/jonathanfiene/Documents/STM32/Core-W Bluetooth"
REPO_DIR="/Users/jonathanfiene/STM32Cube/Repository/STM32Cube_FW_WBA_V1.6.1"
BUILD_DIR="$(cd "$(dirname "$0")" && pwd)/build"
TARGET="core_w_bluetooth"

# Clean and create build dir
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

######################################################################
# Compiler flags - matching CubeIDE Debug configuration
######################################################################

MCU_FLAGS="-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb"

C_DEFS="-DDEBUG -DUSE_HAL_DRIVER -DSTM32WBA55xx -DUSE_FULL_LL_DRIVER -DBLE"

C_INCLUDES=(
    "-I${PROJ_DIR}/Core/Inc"
    "-I${PROJ_DIR}/System/Interfaces"
    "-I${PROJ_DIR}/System/Modules"
    "-I${PROJ_DIR}/System/Config/Log"
    "-I${PROJ_DIR}/System/Config/LowPower"
    "-I${PROJ_DIR}/System/Config/Debug_GPIO"
    "-I${PROJ_DIR}/STM32_WPAN/App"
    "-I${PROJ_DIR}/STM32_WPAN/Target"
    "-I${PROJ_DIR}/System/Config/Flash"
    "-I${PROJ_DIR}/System/Config/ADC_Ctrl"
    "-I${PROJ_DIR}/System/Config/CRC_Ctrl"
    "-I${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Inc"
    "-I${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Inc/Legacy"
    "-I${REPO_DIR}/Utilities/trace/adv_trace"
    "-I${REPO_DIR}/Projects/Common/WPAN/Interfaces"
    "-I${REPO_DIR}/Projects/Common/WPAN/Modules"
    "-I${REPO_DIR}/Projects/Common/WPAN/Modules/BasicAES"
    "-I${REPO_DIR}/Projects/Common/WPAN/Modules/Flash"
    "-I${REPO_DIR}/Projects/Common/WPAN/Modules/MemoryManager"
    "-I${REPO_DIR}/Projects/Common/WPAN/Modules/Nvm"
    "-I${REPO_DIR}/Projects/Common/WPAN/Modules/RTDebug"
    "-I${REPO_DIR}/Projects/Common/WPAN/Modules/SerialCmdInterpreter"
    "-I${REPO_DIR}/Projects/Common/WPAN/Modules/Log"
    "-I${REPO_DIR}/Utilities/misc"
    "-I${REPO_DIR}/Utilities/sequencer"
    "-I${REPO_DIR}/Utilities/tim_serv"
    "-I${REPO_DIR}/Utilities/lpm/tiny_lpm"
    "-I${REPO_DIR}/Middlewares/ST/STM32_WPAN"
    "-I${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/config/ble_basic"
    "-I${REPO_DIR}/Middlewares/ST/STM32_WPAN/ble/svc/Src"
    "-I${REPO_DIR}/Drivers/CMSIS/Device/ST/STM32WBAxx/Include"
    "-I${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/inc"
    "-I${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/inc/_40nm_reg_files"
    "-I${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/inc/ot_inc"
    "-I${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_sys/inc"
    "-I${REPO_DIR}/Middlewares/ST/STM32_WPAN/ble"
    "-I${REPO_DIR}/Middlewares/ST/STM32_WPAN/ble/stack/include"
    "-I${REPO_DIR}/Middlewares/ST/STM32_WPAN/ble/stack/include/auto"
    "-I${REPO_DIR}/Middlewares/ST/STM32_WPAN/ble/svc/Inc"
    "-I${REPO_DIR}/Drivers/CMSIS/Include"
)

CFLAGS="$MCU_FLAGS $C_DEFS -O0 -Wall -ffunction-sections -fdata-sections -fstack-usage -g3 --specs=nano.specs"
ASFLAGS="$MCU_FLAGS $C_DEFS -g3"

######################################################################
# Source files - exactly matching what CubeIDE compiles (from Debug .o list)
######################################################################

C_SOURCES=(
    # Core/Src (project-local)
    "${PROJ_DIR}/Core/Src/app_entry.c"
    "${PROJ_DIR}/Core/Src/main.c"
    "${PROJ_DIR}/Core/Src/stm32wbaxx_hal_msp.c"
    "${PROJ_DIR}/Core/Src/stm32wbaxx_it.c"
    "${PROJ_DIR}/Core/Src/syscalls.c"
    "${PROJ_DIR}/Core/Src/sysmem.c"
    "${PROJ_DIR}/Core/Src/system_stm32wbaxx.c"

    # STM32_WPAN/App (project-local)
    "${PROJ_DIR}/STM32_WPAN/App/app_ble.c"
    "${PROJ_DIR}/STM32_WPAN/App/s1.c"
    "${PROJ_DIR}/STM32_WPAN/App/s1_app.c"
    "${PROJ_DIR}/STM32_WPAN/App/s2.c"
    "${PROJ_DIR}/STM32_WPAN/App/s2_app.c"

    # STM32_WPAN/Target (project-local)
    "${PROJ_DIR}/STM32_WPAN/Target/bleplat.c"
    "${PROJ_DIR}/STM32_WPAN/Target/bpka.c"
    "${PROJ_DIR}/STM32_WPAN/Target/host_stack_if.c"
    "${PROJ_DIR}/STM32_WPAN/Target/linklayer_plat.c"
    "${PROJ_DIR}/STM32_WPAN/Target/ll_sys_if.c"
    "${PROJ_DIR}/STM32_WPAN/Target/power_table.c"

    # System (project-local)
    "${PROJ_DIR}/System/Config/ADC_Ctrl/adc_ctrl_conf.c"
    "${PROJ_DIR}/System/Config/CRC_Ctrl/crc_ctrl_conf.c"
    "${PROJ_DIR}/System/Config/Debug_GPIO/app_debug.c"
    "${PROJ_DIR}/System/Config/Flash/simple_nvm_arbiter_conf.c"
    "${PROJ_DIR}/System/Config/LowPower/peripheral_init.c"
    "${PROJ_DIR}/System/Config/LowPower/user_low_power_config.c"
    "${PROJ_DIR}/System/Interfaces/stm32_lpm_if.c"
    "${PROJ_DIR}/System/Modules/ble_timer.c"

    # Utilities (linked from STM32Cube Repository)
    "${REPO_DIR}/Utilities/trace/adv_trace/stm32_adv_trace.c"
    "${REPO_DIR}/Utilities/lpm/tiny_lpm/stm32_lpm.c"
    "${REPO_DIR}/Utilities/misc/stm32_mem.c"
    "${REPO_DIR}/Utilities/sequencer/stm32_seq.c"
    "${REPO_DIR}/Utilities/misc/stm32_systime.c"
    "${REPO_DIR}/Utilities/tim_serv/stm32_timer.c"
    "${REPO_DIR}/Utilities/misc/stm32_tiny_sscanf.c"
    "${REPO_DIR}/Utilities/misc/stm32_tiny_vsnprintf.c"

    # Drivers/STM32WBAxx_HAL_Driver (linked from STM32Cube Repository)
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_adc.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_adc_ex.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_cortex.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_crc.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_crc_ex.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_dma.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_dma_ex.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_exti.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_flash.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_flash_ex.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_gpio.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_icache.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_pwr.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_pwr_ex.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_ramcfg.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rcc.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rcc_ex.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rng.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rng_ex.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rtc.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rtc_ex.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_uart.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_uart_ex.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_adc.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_dma.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_exti.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_gpio.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_tim.c"
    "${REPO_DIR}/Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_utils.c"

    # Common/WPAN (linked from STM32Cube Repository - Projects/Common)
    "${REPO_DIR}/Projects/Common/WPAN/Interfaces/adv_trace_usart_if.c"
    "${REPO_DIR}/Projects/Common/WPAN/Interfaces/hw_aes.c"
    "${REPO_DIR}/Projects/Common/WPAN/Interfaces/hw_otp.c"
    "${REPO_DIR}/Projects/Common/WPAN/Interfaces/hw_pka.c"
    "${REPO_DIR}/Projects/Common/WPAN/Interfaces/hw_pka_p256.c"
    "${REPO_DIR}/Projects/Common/WPAN/Interfaces/hw_rng.c"
    "${REPO_DIR}/Projects/Common/WPAN/Interfaces/timer_if.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/adc_ctrl.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/app_sys.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/crc_ctrl.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/otp.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/scm.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/stm_list.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/stm_queue.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/temp_measurement.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/BasicAES/baes_ccm.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/BasicAES/baes_cmac.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/BasicAES/baes_ecb.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/Flash/flash_driver.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/Flash/flash_manager.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/Flash/rf_timing_synchro.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/Flash/simple_nvm_arbiter.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/Log/log_module.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/MemoryManager/advanced_memory_manager.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/MemoryManager/stm32_mm.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/Nvm/nvm_emul.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/RTDebug/RTDebug.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/RTDebug/RTDebug_dtb.c"
    "${REPO_DIR}/Projects/Common/WPAN/Modules/SerialCmdInterpreter/serial_cmd_interpreter.c"

    # Middlewares (linked from STM32Cube Repository)
    "${REPO_DIR}/Middlewares/ST/STM32_WPAN/ble/svc/Src/svc_ctl.c"
    "${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_sys/src/ll_sys_cs.c"
    "${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_sys/src/ll_sys_dp_slp.c"
    "${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_sys/src/ll_sys_intf.c"
    "/Users/jonathanfiene/Documents/local/source/cores/sdk/ble/ll_sys_startup.c"
)

ASM_SOURCES=(
    "/Users/jonathanfiene/Documents/local/source/cores/sdk/device/stm32wbaxx/startup_stm32wba55xx.s"
    "${REPO_DIR}/Projects/Common/WPAN/Startup/stm32wbaxx_ResetHandler_GCC.s"
)

######################################################################
# Linker configuration
######################################################################
LDSCRIPT="/Users/jonathanfiene/Documents/local/source/cores/link/stm32wba55hg.ld"

LIBDIRS="-L${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/lib -L${REPO_DIR}/Middlewares/ST/STM32_WPAN/ble/stack/lib"

LIBS="-l:LinkLayer_BLE_Basic_lib.a -l:stm32wba_ble_stack_basic.a"

# Note: LDFLAGS is an array to handle paths with spaces properly
LDFLAGS_ARRAY=(
    $MCU_FLAGS
    "-T${LDSCRIPT}"
    $LIBDIRS
    $LIBS
    --specs=nosys.specs
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,--no-warn-execstack
    -Wl,--no-warn-rwx-segment
    "-Wl,-Map=${BUILD_DIR}/${TARGET}.map,--cref"
    -static
    -lc -lm
)

######################################################################
# Compile
######################################################################

OBJECTS=()
ERRORS=0

echo "============================================"
echo " Compiling working CubeIDE project sources"
echo " with our ARM toolchain"
echo "============================================"
echo ""
echo "Toolchain: $($CC --version | head -1)"
echo "Build dir: $BUILD_DIR"
echo ""

# Compile C sources
echo "--- Compiling ${#C_SOURCES[@]} C source files ---"
for src in "${C_SOURCES[@]}"; do
    base=$(basename "$src" .c)
    obj="${BUILD_DIR}/${base}.o"
    echo "CC  $(basename "$src")"
    if ! $CC -c $CFLAGS "${C_INCLUDES[@]}" "$src" -o "$obj" 2>&1; then
        echo "*** FAILED: $src"
        ERRORS=$((ERRORS + 1))
    fi
    OBJECTS+=("$obj")
done

# Compile ASM sources
echo ""
echo "--- Compiling ${#ASM_SOURCES[@]} assembly files ---"
for src in "${ASM_SOURCES[@]}"; do
    base=$(basename "$src" .s)
    obj="${BUILD_DIR}/${base}.o"
    echo "AS  $(basename "$src")"
    if ! $AS -c -x assembler-with-cpp $ASFLAGS "$src" -o "$obj" 2>&1; then
        echo "*** FAILED: $src"
        ERRORS=$((ERRORS + 1))
    fi
    OBJECTS+=("$obj")
done

if [ $ERRORS -gt 0 ]; then
    echo ""
    echo "*** $ERRORS compilation errors. Stopping."
    exit 1
fi

# Link
echo ""
echo "--- Linking ---"
echo "LD  ${TARGET}.elf"

# Build the object list as a string for the linker
OBJ_LIST=""
for obj in "${OBJECTS[@]}"; do
    OBJ_LIST="$OBJ_LIST $obj"
done

if ! $CC $OBJ_LIST "${LDFLAGS_ARRAY[@]}" -o "${BUILD_DIR}/${TARGET}.elf" 2>&1; then
    echo "*** LINK FAILED"
    exit 1
fi

# Generate binary
echo "BIN ${TARGET}.bin"
$OBJCOPY -O binary "${BUILD_DIR}/${TARGET}.elf" "${BUILD_DIR}/${TARGET}.bin"

echo "HEX ${TARGET}.hex"
$OBJCOPY -O ihex "${BUILD_DIR}/${TARGET}.elf" "${BUILD_DIR}/${TARGET}.hex"

# Size report
echo ""
echo "=== Build Size ==="
$SIZE "${BUILD_DIR}/${TARGET}.elf"

echo ""
echo "=== SUCCESS ==="
echo "ELF: ${BUILD_DIR}/${TARGET}.elf"
echo "BIN: ${BUILD_DIR}/${TARGET}.bin"
echo "HEX: ${BUILD_DIR}/${TARGET}.hex"
echo "MAP: ${BUILD_DIR}/${TARGET}.map"

# Compare with CubeIDE build
if [ -f "${PROJ_DIR}/Debug/Core-W Bluetooth.elf" ]; then
    echo ""
    echo "=== CubeIDE Build Size (for comparison) ==="
    $SIZE "${PROJ_DIR}/Debug/Core-W Bluetooth.elf"
fi
