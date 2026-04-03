/**
 * BLE Beacon — minimal BLE advertising on Core.W
 */

#include "core.h"
#include "stm32_seq.h"

extern void ble_app_init(void);
extern int  ble_app_advertise(const char *name);

/* Debug state readable via CubeProgrammer at &ble_debug[0] */
volatile uint32_t ble_debug[8] __attribute__((used)) = {0};

int main(void)
{
    core_init();
    core_led_init();
    ble_debug[0] = 0xA0;  /* alive */

    ble_app_init();
    ble_debug[0] = 0xA1;  /* init done */

    /* Start advertising from the main loop — aci_gap_set_discoverable
     * is a blocking HCI command that needs the sequencer to drain events. */
    uint8_t adv_started = 0;
    uint32_t loops = 0;

    extern volatile int32_t irq_counter;
    extern volatile uint32_t ble_indication_count;

    while (1) {
        UTIL_SEQ_Run(~0UL);
        loops++;
        ble_debug[2] = loops;
        ble_debug[3] = (uint32_t)irq_counter;
        ble_debug[4] = ble_indication_count;

        /* Heartbeat LED — 500ms toggle */
        {
            static uint32_t last_toggle = 0;
            uint32_t now = core_millis();
            if (now - last_toggle >= 500) {
                last_toggle = now;
                LED_TOGGLE();
            }
        }

        /* Start advertising after sequencer has run a bit */
        if (!adv_started && loops > 100) {
            /* Force-enable interrupts — link layer init leaves them
             * disabled (irq_counter imbalance). Radio ISR needs to fire
             * for HCI command responses to arrive. */
            __asm volatile ("cpsie i" ::: "memory");
            ble_debug[0] = 0xA2;  /* attempting advertise */
            int ret = ble_app_advertise("TILETOWN");
            ble_debug[1] = (uint32_t)ret;
            ble_debug[0] = 0xA3;  /* advertise returned */
            adv_started = 1;
        }
    }
}
