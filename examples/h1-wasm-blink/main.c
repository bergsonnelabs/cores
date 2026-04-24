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

/* WAMR's internal heap. Using a pool (vs. system allocator) sidesteps
 * newlib's malloc — which under -specs=nosys.specs has a stub _sbrk
 * that always fails, silently bricking any runtime that depends on
 * malloc(). 32 KB is enough for our 99-byte module + instance state
 * + operand stack + exec env. Bump if a larger program exhausts it. */
#define WAMR_HEAP_BYTES (32 * 1024)
__attribute__((aligned(8)))
static uint8_t g_wamr_heap[WAMR_HEAP_BYTES];

/* ---------------------------------------------------------------------
 * CRITICAL: the module buffer MUST be writable.
 *
 * WAMR's loader does an in-place optimization during load — see
 * `wasm_const_str_list_insert` in `core/iwasm/interpreter/wasm_runtime.c`.
 * For each import/export name it shifts the string one byte backward
 * in the source buffer (overwriting the leb128 length prefix) and
 * null-terminates it, so the stored `name` can be used as a plain
 * C string with zero allocation.
 *
 * If the buffer is in flash (`.rodata`, typical for an xxd-embedded
 * blob), the writes silently no-op on a Cortex-M — no fault, just no
 * change — and every lookup returns NULL because `name` points at the
 * still-intact length byte. Symptoms: `wasm_runtime_lookup_function`
 * can't find any export, even though `wasm_runtime_get_export_count`
 * reports the right number.
 *
 * Fix: copy the blob into RAM before `wasm_runtime_load`.
 * -------------------------------------------------------------------- */
static uint8_t g_wasm_blob[sizeof(module_wasm)] __attribute__((aligned(4)));

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
    init_args.mem_alloc_type = Alloc_With_Pool;
    init_args.mem_alloc_option.pool.heap_buf = g_wamr_heap;
    init_args.mem_alloc_option.pool.heap_size = sizeof(g_wamr_heap);
    core_usb_printf("  wasm_runtime_full_init...\r\n");
    if (!wasm_runtime_full_init(&init_args)) {
        fatal("wasm_runtime_full_init", "returned false");
    }

    core_usb_printf("  wasm_runtime_register_natives...\r\n");
    if (!wasm_runtime_register_natives(
            "env", g_natives,
            sizeof(g_natives) / sizeof(g_natives[0]))) {
        fatal("wasm_runtime_register_natives", "returned false");
    }

    /* Flash → RAM copy so WAMR's in-place name-rewrite can work. */
    for (size_t i = 0; i < module_wasm_len; i++) {
        g_wasm_blob[i] = module_wasm[i];
    }

    core_usb_printf("  wasm_runtime_load...\r\n");
    char err_buf[96];
    wasm_module_t module = wasm_runtime_load(
        g_wasm_blob, module_wasm_len, err_buf, sizeof(err_buf));
    if (!module) {
        fatal("wasm_runtime_load", err_buf);
    }

    core_usb_printf("  wasm_runtime_instantiate...\r\n");
    wasm_module_inst_t inst = wasm_runtime_instantiate(
        module, /* stack */ 4096, /* heap */ 4096,
        err_buf, sizeof(err_buf));
    if (!inst) {
        wasm_runtime_unload(module);
        fatal("wasm_runtime_instantiate", err_buf);
    }

    core_usb_printf("  wasm_runtime_create_exec_env...\r\n");
    wasm_exec_env_t exec = wasm_runtime_create_exec_env(inst, 4096);
    if (!exec) {
        wasm_runtime_deinstantiate(inst);
        wasm_runtime_unload(module);
        fatal("wasm_runtime_create_exec_env", "returned NULL");
    }

    core_usb_printf("  wasm_runtime_lookup_function...\r\n");
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
    core_usb_printf("  calling tessera_start...\r\n");
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
