/*
 * h1-wasm-blink — A4c proof-of-life on Core.H.
 *
 * Boots the cores SDK, initializes WAMR, loads a 99-byte precompiled
 * Wasm module, registers `core_led_heartbeat` as a host import, and
 * drives `tessera_loop` forever. Each loop iteration has the Wasm
 * module issue `core_led_heartbeat(500)`, which on the cores side
 * toggles the onboard LED and blocks for 500 ms. Net effect: a
 * 1 Hz blink, with every edge round-tripping through the runtime.
 *
 * That's the point of this example: if the LED blinks, the full
 * Wasm-on-device path is alive. If anything — loader, instantiate,
 * native bind, interpreter dispatch — is broken, the LED sits
 * still and the USB CDC log says why.
 */

#include "core.h"
#include "core_led.h"
#include "core_usb.h"

#include "wasm_export.h"

#include "module_wasm.h"

/* -------------------------------------------------------------------
 * Host-import bridge.
 *
 * The Wasm module declares `(import "env" "core_led_heartbeat"
 *  (func (param i32)))` and calls it from its `tessera_loop` body.
 * WAMR's native-call convention puts `wasm_exec_env_t` first, then
 * the declared args in order. We ignore the exec env and forward
 * the period to the real SDK function.
 *
 * The native-signature string `"(i)"` means one i32 parameter, no
 * return value. Each char inside the parens is a param kind, the
 * char after the close-paren (absent here → void) is the return
 * kind. See WAMR's `native_symbol` docs for the full grammar.
 * ----------------------------------------------------------------- */

static void
core_led_heartbeat_native(wasm_exec_env_t env, int32_t period_ms)
{
    (void)env;
    core_led_heartbeat(period_ms);
}

static NativeSymbol g_natives[] = {
    { "core_led_heartbeat", (void *)core_led_heartbeat_native, "(i)", NULL },
};

/* WAMR's wasm_runtime_common.c pulls in a stubbed wasm-c-api
 * dispatcher even when we've disabled the full c-api build. The
 * dead path references wasm_trap_delete; providing a no-op
 * definition here keeps the linker quiet. Never actually called
 * in our configuration. */
typedef struct wasm_trap_t wasm_trap_t;
void wasm_trap_delete(wasm_trap_t *trap) { (void)trap; }

/* -------------------------------------------------------------------
 * WAMR setup + run.
 *
 * Memory sizing notes (tune only if a real program over- or
 * under-flows):
 *   - WAMR operand stack for the module instance: 4 KB.
 *   - WAMR internal heap for the module instance: 4 KB. The mem-alloc
 *     EMS allocator carves this down for small wasm_malloc-ish needs.
 *   - Exec env stack (the interpreter's C-side stack): 4 KB.
 *
 * Total wasm-runtime RAM here is ~12 KB; Core.H has 272 KB so it's
 * fine. This sizing comes up again for Core.W (128 KB) — still
 * fine, but keep an eye on it when larger programs land.
 * ----------------------------------------------------------------- */

static void
usb_warmup(void)
{
    /* Give the host ~1.5 s to enumerate CDC before we start logging;
     * otherwise the early prints land before the serial port is open
     * and the user sees an opaque "nothing happened" boot. */
    core_delay_ms(1500);
}

static void
fatal(const char *tag, const char *detail)
{
    /* One-shot error surface. Print what broke, then flash an SOS
     * pattern forever so the user can see something's wrong without
     * needing the USB log. */
    core_usb_printf("h1-wasm-blink: %s — %s\r\n", tag, detail);
    /* Three fast blinks + pause, forever. `core_led_blink(count,
     * on_ms, off_ms)` handles the pattern; the pause between SOS
     * groups is a separate delay. */
    while (1) {
        core_led_blink(3, 100, 100);
        core_delay_ms(500);
    }
}

int
main(void)
{
    core_init();
    core_usb_init();
    core_led_init();
    usb_warmup();

    core_usb_printf("h1-wasm-blink: booting (WAMR-2.4.4, %u-byte module)\r\n",
                    (unsigned)module_wasm_len);

    RuntimeInitArgs init_args = { 0 };
    init_args.mem_alloc_type = Alloc_With_System_Allocator;
    if (!wasm_runtime_full_init(&init_args)) {
        fatal("wasm_runtime_full_init", "returned false");
    }

    if (!wasm_runtime_register_natives(
            "env", g_natives,
            sizeof(g_natives) / sizeof(g_natives[0]))) {
        fatal("wasm_runtime_register_natives", "returned false");
    }

    char err_buf[96];
    wasm_module_t module = wasm_runtime_load(
        (uint8_t *)module_wasm, module_wasm_len, err_buf, sizeof(err_buf));
    if (!module) {
        fatal("wasm_runtime_load", err_buf);
    }

    wasm_module_inst_t inst = wasm_runtime_instantiate(
        module, /* stack */ 4096, /* heap */ 4096,
        err_buf, sizeof(err_buf));
    if (!inst) {
        wasm_runtime_unload(module);
        fatal("wasm_runtime_instantiate", err_buf);
    }

    wasm_exec_env_t exec = wasm_runtime_create_exec_env(inst, 4096);
    if (!exec) {
        wasm_runtime_deinstantiate(inst);
        wasm_runtime_unload(module);
        fatal("wasm_runtime_create_exec_env", "returned NULL");
    }

    wasm_function_inst_t fn_start =
        wasm_runtime_lookup_function(inst, "tessera_start");
    wasm_function_inst_t fn_loop =
        wasm_runtime_lookup_function(inst, "tessera_loop");
    if (!fn_start || !fn_loop) {
        fatal("wasm_runtime_lookup_function",
              "tessera_start / tessera_loop missing");
    }

    /* tessera_start runs once at boot. Empty in this module, but the
     * infrastructure has to call it anyway — real DSL programs will
     * do one-shot init here (Core.PWM.duty, etc.). */
    if (!wasm_runtime_call_wasm(exec, fn_start, 0, NULL)) {
        fatal("tessera_start",
              wasm_runtime_get_exception(inst));
    }

    core_usb_printf("h1-wasm-blink: running — LED blinks via Wasm\r\n");

    uint32_t tick = 0;
    while (1) {
        if (!wasm_runtime_call_wasm(exec, fn_loop, 0, NULL)) {
            fatal("tessera_loop",
                  wasm_runtime_get_exception(inst));
        }
        if ((++tick % 20) == 0) {
            core_usb_printf("  tick=%-6lu\r\n", (unsigned long)tick);
        }
    }
}
