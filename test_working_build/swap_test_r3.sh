#!/bin/bash
##############################################################################
# Swap Test Round 3 — Use --allow-multiple-definition for symbol conflicts
#
# The key insight: our .o files define some symbols that also exist in the
# working build's .o files (SystemCoreClock, missed_hci_event_flag, etc.).
# Rather than removing entire .o files (which loses other needed symbols),
# we use -Wl,--allow-multiple-definition and let the linker pick the first
# definition. We control priority by putting our .o FIRST in the link order.
#
# This produces valid ELFs we can actually flash to test BLE advertising.
##############################################################################
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKING_BUILD="${SCRIPT_DIR}/build"
HYBRID_DIR="${SCRIPT_DIR}/hybrid_builds_r3"
OUR_REPO="/Users/jonathanfiene/Documents/local/source/cores"
OUR_BLE="${OUR_REPO}/build/Core-W-b/ble-beacon/sdk/ble"
OUR_HAL="${OUR_REPO}/build/Core-W-b/ble-beacon/sdk/hal"
OUR_MAIN="${OUR_REPO}/build/Core-W-b/ble-beacon/projects/ble-beacon"

CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
SIZE=arm-none-eabi-size

PROJ_DIR="/Users/jonathanfiene/Documents/STM32/Core-W Bluetooth"
REPO_DIR="/Users/jonathanfiene/STM32Cube/Repository/STM32Cube_FW_WBA_V1.6.1"

LDSCRIPT="${PROJ_DIR}/STM32WBA55HGFX_FLASH.ld"
MCU_FLAGS="-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb"
LIBDIRS="-L${REPO_DIR}/Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/lib -L${REPO_DIR}/Middlewares/ST/STM32_WPAN/ble/stack/lib"
LIBS="-l:LinkLayer_BLE_Basic_lib.a -l:stm32wba_ble_stack_basic.a"

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

# Link with --allow-multiple-definition
# Our .o files go FIRST so their definitions win over the working build's
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
        -Wl,--allow-multiple-definition \
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
        echo "$link_output" | grep "undefined reference" | head -10
        echo "FAIL  ${name}" >> "$RESULTS_FILE"
        echo "  $(echo "$link_output" | grep "undefined reference" | head -3)" >> "$RESULTS_FILE"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 1
    fi
}

# Build with our .o PREPENDED (wins duplicate resolution) + all working .o
# $1 = name, $2 = comma-separated our .o paths, $3 = comma-separated working .o to REMOVE (optional)
run_swap() {
    local name="$1"
    local our_str="$2"
    local remove_str="$3"

    echo ""
    echo "--- $name ---"

    IFS=',' read -ra ours <<< "$our_str"
    IFS=',' read -ra removes <<< "$remove_str"

    # Our .o first (wins --allow-multiple-definition)
    local objs=()
    for o in "${ours[@]}"; do
        [ -n "$o" ] && objs+=("$o")
    done

    # Then all working .o (minus removals)
    for w in "${WORKING_OBJS[@]}"; do
        local skip=0
        for r in "${removes[@]}"; do
            if [ -n "$r" ] && [ "$w" = "$r" ]; then
                skip=1
                break
            fi
        done
        if [ $skip -eq 0 ]; then
            objs+=("${WORKING_BUILD}/${w}")
        fi
    done

    link_hybrid "$name" "${objs[@]}" || true
}


echo "============================================"
echo " Swap Test Round 3"
echo " --allow-multiple-definition strategy"
echo " Our .o first = our definitions win"
echo "============================================"
echo ""

# Baseline
echo "=== BASELINE ==="
baseline_objs=()
for w in "${WORKING_OBJS[@]}"; do
    baseline_objs+=("${WORKING_BUILD}/${w}")
done
link_hybrid "00_baseline" "${baseline_objs[@]}"

echo ""
echo "=== SINGLE SWAPS (our .o prepended, working .o equivalent removed) ==="

# 1. linklayer_plat — provides SystemCoreClock (conflicts with system_stm32wbaxx)
# With --allow-multiple-definition: our SystemCoreClock wins, working SystemCoreClockUpdate stays
# Remove working linklayer_plat.o to avoid duplicate LINKLAYER_PLAT_* functions
run_swap "01_linklayer_plat" \
    "${OUR_BLE}/linklayer_plat.o" \
    "linklayer_plat.o"

# 2. ll_sys_if
run_swap "02_ll_sys_if" \
    "${OUR_BLE}/ll_sys_if.o" \
    "ll_sys_if.o"

# 3. host_stack_if
run_swap "03_host_stack_if" \
    "${OUR_BLE}/host_stack_if.o" \
    "host_stack_if.o"

# 4. stm_list
run_swap "04_stm_list" \
    "${OUR_BLE}/stm_list.o" \
    "stm_list.o"

# 5. ll_sys_startup — has missed_hci_event_flag conflict with working ll_sys_startup
run_swap "05_ll_sys_startup" \
    "${OUR_BLE}/ll_sys_startup.o" \
    "ll_sys_startup.o"

# 6. bleplat
run_swap "06_bleplat" \
    "${OUR_BLE}/bleplat.o" \
    "bleplat.o"

# 7. power_table
run_swap "07_power_table" \
    "${OUR_BLE}/power_table.o" \
    "power_table.o"

# 8. ble_timer
run_swap "08_ble_timer" \
    "${OUR_BLE}/ble_timer.o" \
    "ble_timer.o"

# 9. svc_ctl
run_swap "09_svc_ctl" \
    "${OUR_BLE}/svc_ctl.o" \
    "svc_ctl.o"

# 10. stm32_mm
run_swap "10_stm32_mm" \
    "${OUR_BLE}/stm32_mm.o" \
    "stm32_mm.o"

# 11. stm32_seq
run_swap "11_stm32_seq" \
    "${OUR_BLE}/stm32_seq.o" \
    "stm32_seq.o"

# 12. stm32_timer
run_swap "12_stm32_timer" \
    "${OUR_BLE}/stm32_timer.o" \
    "stm32_timer.o"

# 13. advanced_memory_manager
run_swap "13_adv_memory_mgr" \
    "${OUR_BLE}/advanced_memory_manager.o" \
    "advanced_memory_manager.o"

echo ""
echo "=== COMBINED SWAPS ==="

# linklayer_plat + ble_irq (they're tightly coupled in our code)
# ble_irq provides radio_callback etc that our linklayer_plat references
# Remove both working equivalents; keep system_stm32wbaxx.o for SystemCoreClockUpdate
run_swap "20_linklayer_plat_ble_irq" \
    "${OUR_BLE}/linklayer_plat.o,${OUR_BLE}/ble_irq.o" \
    "linklayer_plat.o,stm32wbaxx_it.o"

# bleplat + ble_wrap (working bleplat includes ble_wrap code)
run_swap "21_bleplat_ble_wrap" \
    "${OUR_BLE}/bleplat.o,${OUR_BLE}/ble_wrap.o" \
    "bleplat.o"

# ll_sys_startup + host_stack_if (both need missed_hci_event_flag)
# Our ble_glue.o provides missed_hci_event_flag, so include it
run_swap "22_ll_sys_startup_with_glue" \
    "${OUR_BLE}/ll_sys_startup.o,${OUR_BLE}/ble_glue.o" \
    "ll_sys_startup.o"

echo ""
echo "=== PROGRESSIVE COMBOS ==="

# All 9 clean single swaps combined
run_swap "30_all_clean" \
    "${OUR_BLE}/host_stack_if.o,${OUR_BLE}/stm_list.o,${OUR_BLE}/power_table.o,${OUR_BLE}/ble_timer.o,${OUR_BLE}/svc_ctl.o,${OUR_BLE}/stm32_mm.o,${OUR_BLE}/stm32_seq.o,${OUR_BLE}/stm32_timer.o,${OUR_BLE}/advanced_memory_manager.o" \
    "host_stack_if.o,stm_list.o,power_table.o,ble_timer.o,svc_ctl.o,stm32_mm.o,stm32_seq.o,stm32_timer.o,advanced_memory_manager.o"

# All clean + linklayer_plat + ble_irq
run_swap "31_clean_plus_linklayer" \
    "${OUR_BLE}/host_stack_if.o,${OUR_BLE}/stm_list.o,${OUR_BLE}/power_table.o,${OUR_BLE}/ble_timer.o,${OUR_BLE}/svc_ctl.o,${OUR_BLE}/stm32_mm.o,${OUR_BLE}/stm32_seq.o,${OUR_BLE}/stm32_timer.o,${OUR_BLE}/advanced_memory_manager.o,${OUR_BLE}/linklayer_plat.o,${OUR_BLE}/ble_irq.o" \
    "host_stack_if.o,stm_list.o,power_table.o,ble_timer.o,svc_ctl.o,stm32_mm.o,stm32_seq.o,stm32_timer.o,advanced_memory_manager.o,linklayer_plat.o,stm32wbaxx_it.o"

# All clean + linklayer + ll_sys_if + ll_sys_startup + bleplat + ble_wrap
run_swap "32_all_ble_files" \
    "${OUR_BLE}/host_stack_if.o,${OUR_BLE}/stm_list.o,${OUR_BLE}/power_table.o,${OUR_BLE}/ble_timer.o,${OUR_BLE}/svc_ctl.o,${OUR_BLE}/stm32_mm.o,${OUR_BLE}/stm32_seq.o,${OUR_BLE}/stm32_timer.o,${OUR_BLE}/advanced_memory_manager.o,${OUR_BLE}/linklayer_plat.o,${OUR_BLE}/ble_irq.o,${OUR_BLE}/ll_sys_if.o,${OUR_BLE}/ll_sys_startup.o,${OUR_BLE}/bleplat.o,${OUR_BLE}/ble_wrap.o" \
    "host_stack_if.o,stm_list.o,power_table.o,ble_timer.o,svc_ctl.o,stm32_mm.o,stm32_seq.o,stm32_timer.o,advanced_memory_manager.o,linklayer_plat.o,stm32wbaxx_it.o,ll_sys_if.o,ll_sys_startup.o,bleplat.o"

# ALL our BLE .o + ble_glue (maximum replacement)
run_swap "33_all_ours" \
    "${OUR_BLE}/host_stack_if.o,${OUR_BLE}/stm_list.o,${OUR_BLE}/power_table.o,${OUR_BLE}/ble_timer.o,${OUR_BLE}/svc_ctl.o,${OUR_BLE}/stm32_mm.o,${OUR_BLE}/stm32_seq.o,${OUR_BLE}/stm32_timer.o,${OUR_BLE}/advanced_memory_manager.o,${OUR_BLE}/linklayer_plat.o,${OUR_BLE}/ble_irq.o,${OUR_BLE}/ll_sys_if.o,${OUR_BLE}/ll_sys_startup.o,${OUR_BLE}/bleplat.o,${OUR_BLE}/ble_wrap.o,${OUR_BLE}/ble_glue.o" \
    "host_stack_if.o,stm_list.o,power_table.o,ble_timer.o,svc_ctl.o,stm32_mm.o,stm32_seq.o,stm32_timer.o,advanced_memory_manager.o,linklayer_plat.o,stm32wbaxx_it.o,ll_sys_if.o,ll_sys_startup.o,bleplat.o"


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
echo "Hybrid ELFs/BINs in: ${HYBRID_DIR}/"
echo ""
echo "=== FLASH TEST PLAN ==="
echo ""
echo "Phase 1 — Single file swaps (isolate the culprit):"
echo "  Flash 00_baseline.bin first (must advertise)."
echo "  Then flash each single swap. The one that kills advertising is your bug."
echo ""
echo "Phase 2 — Progressive combos (check cumulative effect):"
echo "  30_all_clean       — all safe swaps combined"
echo "  31_clean_plus_linklayer — adds linklayer_plat+ble_irq"
echo "  32_all_ble_files   — adds ll_sys_if, ll_sys_startup, bleplat"
echo "  33_all_ours        — adds ble_glue (max replacement)"
