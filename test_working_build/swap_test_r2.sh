#!/bin/bash
##############################################################################
# Swap Test Round 2 — Fixed cross-dependencies
#
# Round 1 results:
#   PASS (9 clean single swaps): host_stack_if, stm_list, power_table,
#     ble_timer, svc_ctl, stm32_mm, stm32_seq, stm32_timer, adv_memory_mgr
#   FAIL (need combined swaps): linklayer_plat+ble_irq, ll_sys_if,
#     ll_sys_startup, bleplat+ble_wrap, ble_glue (complex)
#
# This round creates properly combined swaps that resolve the cross-deps.
##############################################################################
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORKING_BUILD="${SCRIPT_DIR}/build"
HYBRID_DIR="${SCRIPT_DIR}/hybrid_builds_r2"
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
        echo "$link_output" | grep "multiple definition\|undefined reference" | head -10
        echo "FAIL  ${name}" >> "$RESULTS_FILE"
        echo "  $(echo "$link_output" | grep "multiple definition\|undefined reference" | head -3)" >> "$RESULTS_FILE"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 1
    fi
}

run_swap() {
    local name="$1"
    local remove_str="$2"
    local add_str="$3"

    echo ""
    echo "--- $name ---"

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

    link_hybrid "$name" "${objs[@]}" || true
}

echo "============================================"
echo " Swap Test Round 2 — Combined Swaps"
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
echo "=== ROUND 1 PASSES (re-verify) ==="

# Re-run the 9 clean swaps from round 1 for completeness
for test in \
    "03_host_stack_if:host_stack_if.o:${OUR_BLE}/host_stack_if.o" \
    "04_stm_list:stm_list.o:${OUR_BLE}/stm_list.o" \
    "07_power_table:power_table.o:${OUR_BLE}/power_table.o" \
    "08_ble_timer:ble_timer.o:${OUR_BLE}/ble_timer.o" \
    "09_svc_ctl:svc_ctl.o:${OUR_BLE}/svc_ctl.o" \
    "10_stm32_mm:stm32_mm.o:${OUR_BLE}/stm32_mm.o" \
    "11_stm32_seq:stm32_seq.o:${OUR_BLE}/stm32_seq.o" \
    "12_stm32_timer:stm32_timer.o:${OUR_BLE}/stm32_timer.o" \
    "13_adv_memory_mgr:advanced_memory_manager.o:${OUR_BLE}/advanced_memory_manager.o"
do
    IFS=':' read -r tname tremove tadd <<< "$test"
    run_swap "$tname" "$tremove" "$tadd"
done

echo ""
echo "=== COMBINED SWAPS (fixing round 1 failures) ==="

# Combined 1: linklayer_plat + ble_irq
# Our linklayer_plat.o defines SystemCoreClock (conflicts with system_stm32wbaxx.o)
# Our linklayer_plat.o needs radio_callback from ble_irq.o
# Our ble_irq.o defines radio_callback, low_isr_callback (conflicts with working linklayer_plat.o)
# Working stm32wbaxx_it.o references radio_callback from working linklayer_plat.o
# So we need: our linklayer_plat + our ble_irq, remove working linklayer_plat + stm32wbaxx_it + system_stm32wbaxx
run_swap "20_linklayer_plat_ble_irq" \
    "linklayer_plat.o,stm32wbaxx_it.o,system_stm32wbaxx.o" \
    "${OUR_BLE}/linklayer_plat.o,${OUR_BLE}/ble_irq.o"

# Combined 2: ll_sys_if alone — missing ll_sys_bg_temperature_measurement
# Our ll_sys_if doesn't define this, but working linklayer_plat.o calls it.
# In our build, ll_sys_if provides it differently. Let's check if we also need
# to swap linklayer_plat to use ours (which doesn't call that function).
# Actually wait — the error says working linklayer_plat.c:530 calls ll_sys_bg_temperature_measurement.
# Our ll_sys_if.c doesn't export that symbol. So if we swap both:
run_swap "21_ll_sys_if_with_linklayer" \
    "ll_sys_if.o,linklayer_plat.o,stm32wbaxx_it.o,system_stm32wbaxx.o" \
    "${OUR_BLE}/ll_sys_if.o,${OUR_BLE}/linklayer_plat.o,${OUR_BLE}/ble_irq.o"

# Combined 3: ll_sys_startup — missing missed_hci_event_flag
# Our ll_sys_startup.o no longer exports missed_hci_event_flag (it's in ble_glue).
# Working host_stack_if.o:91 references missed_hci_event_flag.
# So we need: our ll_sys_startup + our ble_glue (which provides missed_hci_event_flag)
# But ble_glue is massive... Let's try with just ll_sys_startup + host_stack_if
run_swap "22_ll_sys_startup_host_stack" \
    "ll_sys_startup.o,host_stack_if.o" \
    "${OUR_BLE}/ll_sys_startup.o,${OUR_BLE}/host_stack_if.o"

# Combined 4: bleplat — missing aci_hal_* and other HCI functions
# Our bleplat.o doesn't include ble_wrap.c code. The working bleplat.o
# apparently includes it (compiled from same source that includes ble_wrap).
# The working app_ble.o calls aci_hal_set_radio_activity_mask etc.
# These come from ble_wrap in the working build (it's included via bleplat.o).
# So: our bleplat + our ble_wrap should provide everything.
# But ble_wrap conflicts with working bleplat's included ble_wrap.
# We need to remove working bleplat.o and add our bleplat + ble_wrap.
run_swap "23_bleplat_ble_wrap" \
    "bleplat.o" \
    "${OUR_BLE}/bleplat.o,${OUR_BLE}/ble_wrap.o"

# Combined 5: ble_glue mega-swap (fixing the missed_hci_event_flag + MX_APPE_* issues)
# ble_glue conflicts with working ll_sys_startup.o on missed_hci_event_flag
# ble_glue doesn't provide MX_APPE_Config/Init/Process (those are in working app_entry.o)
# But we removed app_entry.o... So the working main.o can't find them.
# We need to also remove working main.o since it calls MX_APPE_*.
# But then we'd need to provide main()... from our main.o? That has different deps.
#
# Simpler approach: swap ble_glue + ll_sys_startup (both provide missed_hci_event_flag)
# and also add our main.o which doesn't call MX_APPE_*
run_swap "24_ble_glue_full" \
    "app_entry.o,bpka.o,scm.o,ll_sys_cs.o,ll_sys_dp_slp.o,ll_sys_intf.o,system_stm32wbaxx.o,sysmem.o,stm32_lpm.o,stm32_lpm_if.o,flash_driver.o,flash_manager.o,hw_rng.o,nvm_emul.o,otp.o,simple_nvm_arbiter.o,baes_ccm.o,baes_cmac.o,baes_ecb.o,RTDebug.o,RTDebug_dtb.o,ll_sys_startup.o" \
    "${OUR_BLE}/ble_glue.o,${OUR_BLE}/ll_sys_startup.o"

# Combined 6: ble_glue + ll_sys_startup + remove main (replace with our main)
# This tests what happens when we use our glue + startup + main
run_swap "25_ble_glue_startup_main" \
    "app_entry.o,bpka.o,scm.o,ll_sys_cs.o,ll_sys_dp_slp.o,ll_sys_intf.o,system_stm32wbaxx.o,sysmem.o,stm32_lpm.o,stm32_lpm_if.o,flash_driver.o,flash_manager.o,hw_rng.o,nvm_emul.o,otp.o,simple_nvm_arbiter.o,baes_ccm.o,baes_cmac.o,baes_ecb.o,RTDebug.o,RTDebug_dtb.o,ll_sys_startup.o,main.o" \
    "${OUR_BLE}/ble_glue.o,${OUR_BLE}/ll_sys_startup.o,${OUR_MAIN}/main.o"

echo ""
echo "=== MULTI-FILE PROGRESSIVE SWAPS ==="
echo "(swap more of our files cumulatively to narrow down)"

# Progressive: start with all round-1 passes combined
run_swap "30_all_r1_passes" \
    "host_stack_if.o,stm_list.o,power_table.o,ble_timer.o,svc_ctl.o,stm32_mm.o,stm32_seq.o,stm32_timer.o,advanced_memory_manager.o" \
    "${OUR_BLE}/host_stack_if.o,${OUR_BLE}/stm_list.o,${OUR_BLE}/power_table.o,${OUR_BLE}/ble_timer.o,${OUR_BLE}/svc_ctl.o,${OUR_BLE}/stm32_mm.o,${OUR_BLE}/stm32_seq.o,${OUR_BLE}/stm32_timer.o,${OUR_BLE}/advanced_memory_manager.o"

# Progressive: all r1 passes + linklayer_plat+ble_irq combo
run_swap "31_r1_plus_linklayer_irq" \
    "host_stack_if.o,stm_list.o,power_table.o,ble_timer.o,svc_ctl.o,stm32_mm.o,stm32_seq.o,stm32_timer.o,advanced_memory_manager.o,linklayer_plat.o,stm32wbaxx_it.o,system_stm32wbaxx.o" \
    "${OUR_BLE}/host_stack_if.o,${OUR_BLE}/stm_list.o,${OUR_BLE}/power_table.o,${OUR_BLE}/ble_timer.o,${OUR_BLE}/svc_ctl.o,${OUR_BLE}/stm32_mm.o,${OUR_BLE}/stm32_seq.o,${OUR_BLE}/stm32_timer.o,${OUR_BLE}/advanced_memory_manager.o,${OUR_BLE}/linklayer_plat.o,${OUR_BLE}/ble_irq.o"

# Progressive: all above + ll_sys_if + ll_sys_startup
run_swap "32_r1_plus_linklayer_irq_ll" \
    "host_stack_if.o,stm_list.o,power_table.o,ble_timer.o,svc_ctl.o,stm32_mm.o,stm32_seq.o,stm32_timer.o,advanced_memory_manager.o,linklayer_plat.o,stm32wbaxx_it.o,system_stm32wbaxx.o,ll_sys_if.o,ll_sys_startup.o" \
    "${OUR_BLE}/host_stack_if.o,${OUR_BLE}/stm_list.o,${OUR_BLE}/power_table.o,${OUR_BLE}/ble_timer.o,${OUR_BLE}/svc_ctl.o,${OUR_BLE}/stm32_mm.o,${OUR_BLE}/stm32_seq.o,${OUR_BLE}/stm32_timer.o,${OUR_BLE}/advanced_memory_manager.o,${OUR_BLE}/linklayer_plat.o,${OUR_BLE}/ble_irq.o,${OUR_BLE}/ll_sys_if.o,${OUR_BLE}/ll_sys_startup.o"

# Progressive: all above + bleplat + ble_wrap
run_swap "33_r1_plus_all_ble" \
    "host_stack_if.o,stm_list.o,power_table.o,ble_timer.o,svc_ctl.o,stm32_mm.o,stm32_seq.o,stm32_timer.o,advanced_memory_manager.o,linklayer_plat.o,stm32wbaxx_it.o,system_stm32wbaxx.o,ll_sys_if.o,ll_sys_startup.o,bleplat.o" \
    "${OUR_BLE}/host_stack_if.o,${OUR_BLE}/stm_list.o,${OUR_BLE}/power_table.o,${OUR_BLE}/ble_timer.o,${OUR_BLE}/svc_ctl.o,${OUR_BLE}/stm32_mm.o,${OUR_BLE}/stm32_seq.o,${OUR_BLE}/stm32_timer.o,${OUR_BLE}/advanced_memory_manager.o,${OUR_BLE}/linklayer_plat.o,${OUR_BLE}/ble_irq.o,${OUR_BLE}/ll_sys_if.o,${OUR_BLE}/ll_sys_startup.o,${OUR_BLE}/bleplat.o,${OUR_BLE}/ble_wrap.o"

# Progressive: all above + ble_glue (replaces remaining stubs)
run_swap "34_all_ble_plus_glue" \
    "host_stack_if.o,stm_list.o,power_table.o,ble_timer.o,svc_ctl.o,stm32_mm.o,stm32_seq.o,stm32_timer.o,advanced_memory_manager.o,linklayer_plat.o,stm32wbaxx_it.o,system_stm32wbaxx.o,ll_sys_if.o,ll_sys_startup.o,bleplat.o,app_entry.o,bpka.o,scm.o,ll_sys_cs.o,ll_sys_dp_slp.o,ll_sys_intf.o,sysmem.o,stm32_lpm.o,stm32_lpm_if.o,flash_driver.o,flash_manager.o,hw_rng.o,nvm_emul.o,otp.o,simple_nvm_arbiter.o,baes_ccm.o,baes_cmac.o,baes_ecb.o,RTDebug.o,RTDebug_dtb.o" \
    "${OUR_BLE}/host_stack_if.o,${OUR_BLE}/stm_list.o,${OUR_BLE}/power_table.o,${OUR_BLE}/ble_timer.o,${OUR_BLE}/svc_ctl.o,${OUR_BLE}/stm32_mm.o,${OUR_BLE}/stm32_seq.o,${OUR_BLE}/stm32_timer.o,${OUR_BLE}/advanced_memory_manager.o,${OUR_BLE}/linklayer_plat.o,${OUR_BLE}/ble_irq.o,${OUR_BLE}/ll_sys_if.o,${OUR_BLE}/ll_sys_startup.o,${OUR_BLE}/bleplat.o,${OUR_BLE}/ble_wrap.o,${OUR_BLE}/ble_glue.o"

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
echo "Hybrid ELFs/BINs: ${HYBRID_DIR}/"
echo ""
echo "Flash test order:"
echo "  1. 00_baseline.bin — sanity (should advertise)"
echo "  2. Each passing single swap — find which one kills advertising"
echo "  3. Progressive combos — find the smallest set that kills it"
