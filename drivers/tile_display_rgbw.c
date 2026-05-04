/**
 * @file   tile_display_rgbw.c
 * @brief  Disp.RGBW (LP5811) — platform-agnostic driver.
 */

#include "tile_display_rgbw.h"
#include <stddef.h>

/* ---- Instance table ---- */

static const uint8_t id_table[] = { LP5811_I2C_ADDR_DEFAULT };
#define NUM_INSTANCES  (sizeof(id_table) / sizeof(id_table[0]))

static uint8_t resolve_id(uint8_t instance)
{
    return (instance < NUM_INSTANCES) ? id_table[instance] : 0;
}

/* ---- Private helpers ---- */

static void lp_write(tile_t *tile, uint8_t reg, uint8_t val)
{
    tile->hal->i2c_write(tile->hal->handle, tile->id, reg, &val, 1);
}

static uint8_t lp_read(tile_t *tile, uint8_t reg)
{
    uint8_t val = 0;
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, &val, 1);
    return val;
}

/* The LP5811's register address is 10 bits wide. The lower 8 bits
 * sit in the I2C "register byte"; the upper 2 bits ("page") are
 * encoded into bits[1:0] of the 7-bit chip address. Status registers
 * we care about live on page 3 (addresses 0x300+), so we read them
 * via tile->id | 0x03. See LP5811 datasheet §7.5 — Programming. */
static uint8_t lp_read_page(tile_t *tile, uint8_t page, uint8_t reg)
{
    uint8_t val = 0;
    uint8_t paged_id = (tile->id & ~0x03) | (page & 0x03);
    tile->hal->i2c_read(tile->hal->handle, paged_id, reg, &val, 1);
    return val;
}

/** Latch any Dev_Config_* writes — required by the chip per
 *  datasheet §2.4.1 (CMD_Update). Writing 0x55 to 0x10 commits
 *  registers 0x001..0x00B. */
static void lp_commit(tile_t *tile)
{
    lp_write(tile, LP5811_REG_CMD_UPDATE, 0x55);
}

/* ---- Public API ---- */

uint8_t tile_display_rgbw_find(tiles_pal_t *hal, uint8_t instance)
{
    uint8_t addr = resolve_id(instance);
    if (!addr) return 0;
    return hal->i2c_is_ready(hal->handle, addr) == 0;
}

void tile_display_rgbw_init(tiles_pal_t *hal, uint8_t instance, tile_t *tile,
                         const disp_rgbw_cfg_t *cfg)
{
    (void)cfg;  /* Reserved for future use */
    for (uint8_t i = 0; i < sizeof(tile_t); i++)
        ((uint8_t *)tile)[i] = 0;

    tile->hal = hal;
    tile->id  = resolve_id(instance);

    if (!tile->id) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "disp_rgbw: invalid instance");
        return;
    }

    if (hal->i2c_is_ready(hal->handle, tile->id) != 0) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "disp_rgbw: device not found");
        return;
    }

    /* Enable chip */
    lp_write(tile, LP5811_REG_CHIP_EN, 0x01);
    hal->delay_ms(2);

    /* Verify chip is alive */
    uint8_t cfg2 = lp_read(tile, LP5811_REG_CONFIG_2);
    if (cfg2 != LP5811_CONFIG_2_DEFAULT) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "disp_rgbw: CONFIG_2 verify failed");
        return;
    }

    /* Boost voltage 4.5V (boost_vout = 0x0F), max current 51 mA (MC=1) */
    lp_write(tile, LP5811_REG_CONFIG_0, 0x1F);

    /* Dev_Config_12: clamp default (vmid_sel=0, clamp_sel=0, clamp_dis=0),
     * lod_action=1 (open shuts down sink), lsd_action=0 (short reports
     * only — driver-level choice; firmware can opt-in via set_short_shutdown),
     * lsd_threshold=3 (0.65 × VOUT, most permissive). */
    lp_write(tile, LP5811_REG_CONFIG_12, 0x0B);

    /* Commit config */
    lp_commit(tile);

    /* Enable all 4 LED channels */
    lp_write(tile, LP5811_REG_LED_EN, 0x0F);

    /* Default current limits: 50% */
    lp_write(tile, LP5811_REG_DC_0, 0x80);
    lp_write(tile, LP5811_REG_DC_1, 0x80);
    lp_write(tile, LP5811_REG_DC_2, 0x80);
    lp_write(tile, LP5811_REG_DC_3, 0x80);

    tile->state = TILE_STATE_READY;
}

void tile_display_rgbw_set(tile_t *tile, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    /* Channel mapping: LED0=R, LED1=B, LED2=G, LED3=W */
    lp_write(tile, LP5811_REG_PWM_0, r);
    lp_write(tile, LP5811_REG_PWM_1, b);
    lp_write(tile, LP5811_REG_PWM_2, g);
    lp_write(tile, LP5811_REG_PWM_3, w);
}

void tile_display_rgbw_off(tile_t *tile)
{
    lp_write(tile, LP5811_REG_PWM_0, 0);
    lp_write(tile, LP5811_REG_PWM_1, 0);
    lp_write(tile, LP5811_REG_PWM_2, 0);
    lp_write(tile, LP5811_REG_PWM_3, 0);
}

void tile_display_rgbw_set_current(tile_t *tile, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    lp_write(tile, LP5811_REG_DC_0, r);
    lp_write(tile, LP5811_REG_DC_1, b);
    lp_write(tile, LP5811_REG_DC_2, g);
    lp_write(tile, LP5811_REG_DC_3, w);
}

void tile_display_rgbw_set_max_current(tile_t *tile, disp_rgbw_max_current_t mode)
{
    /* Read-modify-write Dev_Config_0 — preserves boost_vout. Only bit 0
     * is the MC selector. */
    uint8_t cfg0 = lp_read(tile, LP5811_REG_CONFIG_0);
    cfg0 = (cfg0 & ~0x01u) | (mode ? 0x01u : 0x00u);
    lp_write(tile, LP5811_REG_CONFIG_0, cfg0);
    lp_commit(tile);
}

void tile_display_rgbw_read_faults(tile_t *tile, disp_rgbw_faults_t *out)
{
    if (!out) return;
    /* Zero on entry so partial bus failures don't leak garbage. */
    out->open_mask        = 0;
    out->short_mask       = 0;
    out->thermal_shutdown = 0;
    out->config_error     = 0;

    uint8_t tsd = lp_read_page(tile, 3, LP5811_REG_TSD_STATUS);
    uint8_t lod = lp_read_page(tile, 3, LP5811_REG_LOD_STATUS_0);
    uint8_t lsd = lp_read_page(tile, 3, LP5811_REG_LSD_STATUS_0);

    out->config_error     = (tsd & 0x01) ? 1 : 0;
    out->thermal_shutdown = (tsd & 0x02) ? 1 : 0;
    out->open_mask  = lod & 0x0F;
    out->short_mask = lsd & 0x0F;
}

void tile_display_rgbw_clear_faults(tile_t *tile)
{
    /* Fault_Clear (0x22) is W1C: bit2=tsd, bit1=lsd, bit0=lod. */
    lp_write(tile, LP5811_REG_FAULT_CLEAR, 0x07);
}

void tile_display_rgbw_set_short_threshold(tile_t *tile,
                                           disp_rgbw_lsd_threshold_t threshold)
{
    uint8_t cfg12 = lp_read(tile, LP5811_REG_CONFIG_12);
    cfg12 = (cfg12 & ~0x03u) | ((uint8_t)threshold & 0x03u);
    lp_write(tile, LP5811_REG_CONFIG_12, cfg12);
    lp_commit(tile);
}

void tile_display_rgbw_set_short_shutdown(tile_t *tile, uint8_t enabled)
{
    /* Dev_Config_12 bit 2 = lsd_action. */
    uint8_t cfg12 = lp_read(tile, LP5811_REG_CONFIG_12);
    cfg12 = (cfg12 & ~0x04u) | (enabled ? 0x04u : 0x00u);
    lp_write(tile, LP5811_REG_CONFIG_12, cfg12);
    lp_commit(tile);
}

void tile_display_rgbw_set_open_shutdown(tile_t *tile, uint8_t enabled)
{
    /* Dev_Config_12 bit 3 = lod_action. */
    uint8_t cfg12 = lp_read(tile, LP5811_REG_CONFIG_12);
    cfg12 = (cfg12 & ~0x08u) | (enabled ? 0x08u : 0x00u);
    lp_write(tile, LP5811_REG_CONFIG_12, cfg12);
    lp_commit(tile);
}

void tile_display_rgbw_sleep(tile_t *tile)
{
    lp_write(tile, LP5811_REG_CHIP_EN, 0x00);
    tile->state = TILE_STATE_SLEEPING;
}

void tile_display_rgbw_wake(tile_t *tile)
{
    lp_write(tile, LP5811_REG_CHIP_EN, 0x01);
    tile->hal->delay_ms(2);
    tile->state = TILE_STATE_READY;
}

void tile_display_rgbw_reset(tile_t *tile)
{
    lp_write(tile, LP5811_REG_RESET, 0x66);  /* per datasheet §2.7.1: write 0x66 */
    tile->hal->delay_ms(2);
    tile->state = TILE_STATE_NONE;
}

/* ---- Tier-2 idiomatic helpers ---- */

void tile_display_rgbw_set_color(tile_t *tile, uint8_t r, uint8_t g, uint8_t b)
{
    tile_display_rgbw_set(tile, r, g, b, 0);
}

void tile_display_rgbw_pulse(tile_t *tile, uint8_t r, uint8_t g, uint8_t b,
                             uint16_t ms)
{
    tile_display_rgbw_set(tile, r, g, b, 0);
    tile->hal->delay_ms(ms);
    tile_display_rgbw_off(tile);
}

void tile_display_rgbw_breathe(tile_t *tile, uint8_t r, uint8_t g, uint8_t b,
                               uint16_t period_ms)
{
    /* Software ramp — 32 steps up, 32 steps down (64 total).
     * On-chip AEU could do this autonomously, but its bytecode is not
     * publicly documented (see header @studio unsupported note). */
    const uint8_t STEPS = 32;
    uint16_t step_ms = period_ms / (uint16_t)(STEPS * 2u);
    if (step_ms == 0) step_ms = 1;  /* clamp — too-short period falls back to choppy */

    /* Ramp up: 0 → peak. */
    for (uint8_t i = 1; i <= STEPS; i++) {
        uint8_t scale_r = (uint8_t)(((uint16_t)r * i) / STEPS);
        uint8_t scale_g = (uint8_t)(((uint16_t)g * i) / STEPS);
        uint8_t scale_b = (uint8_t)(((uint16_t)b * i) / STEPS);
        tile_display_rgbw_set(tile, scale_r, scale_g, scale_b, 0);
        tile->hal->delay_ms(step_ms);
    }

    /* Ramp down: peak → 0. */
    for (uint8_t i = STEPS; i > 0; i--) {
        uint8_t j = (uint8_t)(i - 1u);
        uint8_t scale_r = (uint8_t)(((uint16_t)r * j) / STEPS);
        uint8_t scale_g = (uint8_t)(((uint16_t)g * j) / STEPS);
        uint8_t scale_b = (uint8_t)(((uint16_t)b * j) / STEPS);
        tile_display_rgbw_set(tile, scale_r, scale_g, scale_b, 0);
        tile->hal->delay_ms(step_ms);
    }

    /* Guarantee fully off at end (rounding could leave a sliver). */
    tile_display_rgbw_off(tile);
}

void tile_display_rgbw_flash(tile_t *tile, uint8_t r, uint8_t g, uint8_t b,
                             uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        tile_display_rgbw_set(tile, r, g, b, 0);
        tile->hal->delay_ms(100);
        tile_display_rgbw_off(tile);
        tile->hal->delay_ms(100);
    }
}

uint8_t tile_display_rgbw_is_faulted(tile_t *tile)
{
    disp_rgbw_faults_t f;
    tile_display_rgbw_read_faults(tile, &f);
    if (f.open_mask || f.short_mask || f.thermal_shutdown || f.config_error) {
        return 1;
    }
    return 0;
}
