#!/bin/bash
##############################################################################
# Binary Object File Swap Test
#
# Starts with the WORKING build's .o files and swaps in ONE of our
# pre-compiled .o files at a time. Links with the working build's linker
# script and libraries.
#
# If a hybrid links cleanly, flash it to test BLE advertising.
# The swap that breaks advertising is the culprit source file.
##############################################################################
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKING_BUILD="${SCRIPT_DIR}/build"
HYBRID_DIR="${SCRIPT_DIR}/hybrid_builds"
OUR_REPO="/Users/jonathanfiene/Documents/local/source/cores"
OUR_BLE="${OUR_REPO}/build/Core-W-b/ble-beacon/sdk/ble"
OUR_HAL="${OUR_REPO}/build/Core-W-b/ble-beacon/sdk/hal"
OUR_MAIN="${OUR_REPO}/build/Core-W-b/ble-beacon/projects/ble-beacon"

# Toolchain
CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
SIZE=arm-none-eabi-size

# Paths for working project
PROJ_DIR="/Users/jonathanfiene/Documents/STM32/Core-W Bluetooth"
REPO_DIR="/Users/jonathanfiene/STM32Cube/Repository/STM32Cube_FW_WBA_V1.6.1"

# Working build's linker config
LDSCRIPT="${PROJ_DIR}/STM32WBA55HGFX_FLASH.ld"
MCU_FLAGS="-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb"

LIBDIRS="-L${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/lib -L${REPO_DIR}/Middlewares/ST/STM32_WPAN/ble/stack/lib"
LIBS="-l:LinkLayer_BLE_Basic_lib.a -l:stm32wba_ble_stack_basic.a"

# All working .o files (the baseline)
WORKING_OBJS=(
    RTDebug.o RTDebug_dtb.o adc_ctrl.o adc_ctrl_conf.o adv_trace_usart_if.o
    advanced_memory_manager.o app_ble.o app_debug.o app_entry.o app_sys.o
    baes_ccm.o baes_cmac.o baes_ecb.o ble_timer.o bleplat.o bpka.o
    crc_ctrl.o crc_ctrl_conf.o flash_driver.o flash_manager.o
    host_stack_if.o hw_aes.o hw_otp.o hw_pka.o hw_pka_p256.o hw_rng.o
    linklayer_plat.o ll_sys_cs.o ll_sys_dp_slp.o ll_sys_if.o
    ll_sys_intf.o ll_sys_startup.o log_module.o main.o nvm_emul.o otp.o
    peripheral_init.o power_table.o rf_timing_synchro.o
    s1.o s1_app.o s2.o s2_app.o scm.o serial_cmd_interpreter.o
    simple_nvm_arbiter.o simple_nvm_arbiter_conf.o
    startup_stm32wba55hgfx.o stm32_adv_trace.o stm32_lpm.o stm32_lpm_if.o
    stm32_mem.o stm32_mm.o stm32_seq.o stm32_systime.o stm32_timer.o
    stm32_tiny_sscanf.o stm32_tiny_vsnprintf.o stm32wbaxx_ResetHandler_GCC.o
    stm32wbaxx_hal.o stm32wbaxx_hal_adc.o stm32wbaxx_hal_adc_ex.o
    stm32wbaxx_hal_cortex.o stm32wbaxx_hal_crc.o stm32wbaxx_hal_crc_ex.o
    stm32wbaxx_hal_dma.o stm32wbaxx_hal_dma_ex.o stm32wbaxx_hal_exti.o
    stm32wbaxx_hal_flash.o stm32wbaxx_hal_flash_ex.o stm32wbaxx_hal_gpio.o
    stm32wbaxx_hal_icache.o stm32wbaxx_hal_msp.o stm32wbaxx_hal_pwr.o
    stm32wbaxx_hal_pwr_ex.o stm32wbaxx_hal_ramcfg.o stm32wbaxx_hal_rcc.o
    stm32wbaxx_hal_rcc_ex.o stm32wbaxx_hal_rng.o stm32wbaxx_hal_rng_ex.o
    stm32wbaxx_hal_rtc.o stm32wbaxx_hal_rtc_ex.o stm32wbaxx_hal_uart.o
    stm32wbaxx_hal_uart_ex.o stm32wbaxx_it.o stm32wbaxx_ll_adc.o
    stm32wbaxx_ll_dma.o stm32wbaxx_ll_exti.o stm32wbaxx_ll_gpio.o
    stm32wbaxx_ll_tim.o stm32wbaxx_ll_utils.o stm_list.o stm_queue.o
    svc_ctl.o syscalls.o sysmem.o system_stm32wbaxx.o temp_measurement.o
    timer_if.o user_low_power_config.o
)

rm -rf "$HYBRID_DIR"
mkdir -p "$HYBRID_DIR"

RESULTS_FILE="${HYBRID_DIR}/results.txt"
> "$RESULTS_FILE"

PASS_COUNT=0
FAIL_COUNT=0

# Helper: link a set of .o files into an ELF
link_hybrid() {
    local name="$1"
    shift
    local objs=("$@")
    local elf="${HYBRID_DIR}/${name}.elf"

    local obj_list=""
    for obj in "${objs[@]}"; do
        obj_list="$obj_list $obj"
    done

    local link_output
    link_output=$($CC $obj_list $MCU_FLAGS "-T${LDSCRIPT}" $LIBDIRS $LIBS \
        --specs=nosys.specs --specs=nano.specs \
        -Wl,--gc-sections \
        -Wl,--no-warn-execstack \
        -Wl,--no-warn-rwx-segment \
        "-Wl,-Map=${HYBRID_DIR}/${name}.map,--cref" \
        -static -lc -lm \
        -o "$elf" 2>&1)
    local rc=$?

    if [ $rc -eq 0 ]; then
        local size_info=$($SIZE "$elf" | tail -1)
        echo "  LINK OK  $size_info"
        echo "PASS  ${name}  $size_info" >> "$RESULTS_FILE"
        $OBJCOPY -O binary "$elf" "${HYBRID_DIR}/${name}.bin" 2>/dev/null
        $OBJCOPY -O ihex "$elf" "${HYBRID_DIR}/${name}.hex" 2>/dev/null
        PASS_COUNT=$((PASS_COUNT + 1))
        return 0
    else
        echo "  LINK FAIL"
        echo "$link_output" | head -20
        echo "FAIL  ${name}" >> "$RESULTS_FILE"
        echo "  $(echo "$link_output" | grep "multiple definition\|undefined reference" | head -5)" >> "$RESULTS_FILE"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 1
    fi
}

# Helper: build object list removing specified files and adding specified files
build_obj_list() {
    # Args: remove_list add_list
    # remove_list is comma-separated basenames
    # add_list is comma-separated full paths
    local remove_str="$1"
    local add_str="$2"

    IFS=',' read -ra removes <<< "$remove_str"
    IFS=',' read -ra adds <<< "$add_str"

    local objs=()
    for w in "${WORKING_OBJS[@]}"; do
        local skip=0
        for r in "${removes[@]}"; do
            if [ "$w" = "$r" ]; then
                skip=1
                break
            fi
        done
        if [ $skip -eq 0 ]; then
            objs+=("${WORKING_BUILD}/${w}")
        fi
    done
    for a in "${adds[@]}"; do
        objs+=("$a")
    done

    echo "${objs[@]}"
}

run_swap() {
    local name="$1"
    local remove_str="$2"  # comma-separated working .o basenames to remove
    local add_str="$3"     # comma-separated paths to our .o files to add

    echo ""
    echo "--- $name ---"
    echo "  Remove: $remove_str"
    echo "  Add:    $(echo "$add_str" | sed 's|.*/||g; s|,|, |g')"

    local obj_str
    obj_str=$(build_obj_list "$remove_str" "$add_str")
    read -ra obj_array <<< "$obj_str"

    link_hybrid "$name" "${obj_array[@]}" || true
}


echo "============================================"
echo " Binary Object File Swap Test"
echo " Using pre-compiled .o files from both builds"
echo "============================================"
echo "Toolchain: $($CC --version | head -1)"
echo ""

# Baseline: working build re-linked
echo "=== BASELINE ==="
baseline_objs=()
for w in "${WORKING_OBJS[@]}"; do
    baseline_objs+=("${WORKING_BUILD}/${w}")
done
link_hybrid "00_baseline" "${baseline_objs[@]}"

echo ""
echo "============================================"
echo " SIMPLE 1-TO-1 SWAPS"
echo " (our .o directly replaces same-named working .o)"
echo "============================================"

# These are direct drops-in: same function names, same API.
# Our .o was compiled with our flags/headers but provides the same symbols.

run_swap "01_linklayer_plat" \
    "linklayer_plat.o" \
    "${OUR_BLE}/linklayer_plat.o"

run_swap "02_ll_sys_if" \
    "ll_sys_if.o" \
    "${OUR_BLE}/ll_sys_if.o"

run_swap "03_host_stack_if" \
    "host_stack_if.o" \
    "${OUR_BLE}/host_stack_if.o"

run_swap "04_stm_list" \
    "stm_list.o" \
    "${OUR_BLE}/stm_list.o"

run_swap "05_ll_sys_startup" \
    "ll_sys_startup.o" \
    "${OUR_BLE}/ll_sys_startup.o"

run_swap "06_bleplat" \
    "bleplat.o" \
    "${OUR_BLE}/bleplat.o"

run_swap "07_power_table" \
    "power_table.o" \
    "${OUR_BLE}/power_table.o"

run_swap "08_ble_timer" \
    "ble_timer.o" \
    "${OUR_BLE}/ble_timer.o"

run_swap "09_svc_ctl" \
    "svc_ctl.o" \
    "${OUR_BLE}/svc_ctl.o"

run_swap "10_stm32_mm" \
    "stm32_mm.o" \
    "${OUR_BLE}/stm32_mm.o"

run_swap "11_stm32_seq" \
    "stm32_seq.o" \
    "${OUR_BLE}/stm32_seq.o"

run_swap "12_stm32_timer" \
    "stm32_timer.o" \
    "${OUR_BLE}/stm32_timer.o"

run_swap "13_adv_memory_mgr" \
    "advanced_memory_manager.o" \
    "${OUR_BLE}/advanced_memory_manager.o"

echo ""
echo "============================================"
echo " COMPLEX SWAPS"
echo " (our .o replaces multiple working .o files)"
echo "============================================"

# ble_glue.o replaces many working stubs.
# It provides symbols from: app_entry, bpka, scm, ll_sys_cs, ll_sys_dp_slp,
# ll_sys_intf, system_stm32wbaxx, sysmem, stm32_lpm, stm32_lpm_if,
# flash_driver, flash_manager, hw_rng, nvm_emul, otp, simple_nvm_arbiter,
# baes_ccm, baes_cmac, baes_ecb, RTDebug, RTDebug_dtb
run_swap "14_ble_glue" \
    "app_entry.o,bpka.o,scm.o,ll_sys_cs.o,ll_sys_dp_slp.o,ll_sys_intf.o,system_stm32wbaxx.o,sysmem.o,stm32_lpm.o,stm32_lpm_if.o,flash_driver.o,flash_manager.o,hw_rng.o,nvm_emul.o,otp.o,simple_nvm_arbiter.o,baes_ccm.o,baes_cmac.o,baes_ecb.o,RTDebug.o,RTDebug_dtb.o" \
    "${OUR_BLE}/ble_glue.o"

# ble_irq.o replaces stm32wbaxx_it.o (interrupt handlers)
# Our ble_irq provides: RADIO_IRQHandler, HASH_IRQHandler
# Working stm32wbaxx_it.o provides those PLUS many more IRQ handlers.
# This will likely fail due to missing IRQ handlers that the working build had.
run_swap "15_ble_irq" \
    "stm32wbaxx_it.o" \
    "${OUR_BLE}/ble_irq.o"

# stm32_timer_if.o replaces timer_if.o
run_swap "16_timer_if" \
    "timer_if.o" \
    "${OUR_BLE}/stm32_timer_if.o"

# ble_wrap.o — this is the HCI/GAP/GATT wrapper. The working build doesn't have
# a separate equivalent — these functions come from the static BLE stack library.
# ble_wrap.o shouldn't conflict with anything in the working build.
# Let's try just ADDING it (no removal) to see if it links.
run_swap "17_ble_wrap_add" \
    "" \
    "${OUR_BLE}/ble_wrap.o"

echo ""
echo ""
echo "============================================"
echo " RESULTS SUMMARY"
echo "============================================"
echo ""
cat "$RESULTS_FILE"
echo ""
echo "Passed: $PASS_COUNT   Failed: $FAIL_COUNT"
echo ""
echo "--- NEXT STEPS ---"
echo "For each PASS hybrid, flash and check BLE scanner:"
echo "  STM32_Programmer_CLI -c port=SWD -d <name>.bin 0x08000000 -rst"
echo ""
echo "If baseline advertises but swap_X doesn't, then X is the culprit."
echo ""
echo "Hybrid ELFs/BINs: ${HYBRID_DIR}/"
