/**
 * val-watchdog-u -- Validation: Watchdog + fault callback on Core.U
 *
 * Core.U (Core-U-2-a), clock=max
 *
 * Exercises: core_init, core_watchdog_start, core_watchdog_feed,
 *            core_watchdog_caused_reset, core_watchdog_clear_flags,
 *            core_fault_set_callback (if available)
 *
 * Note: core_fault_set_callback is a planned API. It is guarded with
 * #ifdef so that this test compiles regardless of whether the fault
 * handler hook has been implemented yet. The watchdog APIs are fully
 * implemented and are the primary validation target.
 */

#include "core.h"
#include "core_watchdog.h"

/* Fault callback -- expected hal_fault_callback_t signature: void(void *ctx)
 * This is the callback shape that core_fault_set_callback will accept
 * once the fault-handler hook is implemented. */
static void my_fault_cb(void *ctx)
{
    (void)ctx;
    /* In a real application: log fault info, blink LED pattern, etc. */
}

int main(void)
{
    core_init();
    core_led_init();

    /* Check if the last reset was caused by the watchdog */
    if (core_watchdog_caused_reset()) {
        /* Clear all reset flags */
        core_watchdog_clear_flags();

        /* Indicate watchdog recovery */
        for (int i = 0; i < 10; i++) {
            LED_TOGGLE();
            core_delay_ms(50);
        }
    }

    /* Register fault callback if the API exists */
#ifdef core_fault_set_callback
    core_fault_set_callback(my_fault_cb);
#else
    /* Suppress unused warning -- fault API not yet available */
    (void)my_fault_cb;
#endif

    /* Start independent watchdog -- 2 second timeout */
    core_watchdog_start(2000);

    while (1) {
        LED_TOGGLE();
        core_delay_ms(500);

        /* Feed the watchdog to prevent reset */
        core_watchdog_feed();
    }
}
