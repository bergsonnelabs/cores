/**
 * @file   tile_sense_t_c.c
 * @brief  Sense.T.C (IQS323) -- platform-agnostic driver implementation.
 *
 * Platform-agnostic. All bus access via tile->hal function pointers.
 * Reference: Azoteq IQS323 Datasheet v1.11, August 2025.
 */

#include "tile_sense_t_c.h"
#include <stddef.h>

/* ================================================================
 * Instance -> I2C address table
 * ================================================================ */

static const uint8_t id_table[] = {
    IQS323_I2C_ADDR_001,   /* 0: 0x44 (order code 001) */
    IQS323_I2C_ADDR_002,   /* 1: 0x58 (order code 002) */
};

#define NUM_INSTANCES  (sizeof(id_table) / sizeof(id_table[0]))

static uint8_t resolve_id(uint8_t instance)
{
    return (instance < NUM_INSTANCES) ? id_table[instance] : 0;
}

/* ================================================================
 * Per-instance driver state
 * ================================================================ */

typedef struct {
    sense_t_c_event_cb_t on_event;
    void *event_ctx;
    uint16_t last_status;          /* Previous status for edge detection */
    volatile uint8_t rdy_flag;     /* Set by EXTI ISR */
    uint8_t rdy_pin;               /* Core pad number, 0 = polled */
    uint8_t prox_threshold;
    uint8_t touch_threshold;
} iqs323_state_t;

static iqs323_state_t state[NUM_INSTANCES];

static iqs323_state_t *state_for(tile_t *tile)
{
    for (uint8_t i = 0; i < NUM_INSTANCES; i++)
        if (id_table[i] == tile->id) return &state[i];
    return &state[0];
}

/* ================================================================
 * Private helpers
 * ================================================================ */

static void memzero(void *p, uint16_t n)
{
    uint8_t *b = (uint8_t *)p;
    while (n--) *b++ = 0;
}

/**
 * Force a communication window open by sending 0xFF as a raw data byte.
 * Only works when hal->i2c_write_raw is available.
 */
static void iqs_force_comms(tile_t *tile)
{
    if (tile->hal->i2c_write_raw) {
        uint8_t cmd = 0xFF;
        tile->hal->i2c_write_raw(tile->hal->handle, tile->id, &cmd, 1);
        tile->hal->delay_ms(15);  /* t_wait: 0.1–45ms per datasheet */
    }
}

/** Read a 16-bit little-endian register. May return 0xEEEE outside window. */
static uint16_t iqs_read(tile_t *tile, uint8_t reg)
{
    uint8_t buf[2] = {0xEE, 0xEE};
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, buf, 2);
    return ((uint16_t)buf[1] << 8) | buf[0];
}

/** Write a 16-bit little-endian register. */
static void iqs_write(tile_t *tile, uint8_t reg, uint16_t value)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(value & 0xFF);
    buf[1] = (uint8_t)((value >> 8) & 0xFF);
    tile->hal->i2c_write(tile->hal->handle, tile->id, reg, buf, 2);
}

/** Read with force-comms + retry until non-0xEEEE or timeout. */
static uint16_t iqs_read_window(tile_t *tile, uint8_t reg)
{
    for (int attempt = 0; attempt < 10; attempt++) {
        iqs_force_comms(tile);
        uint16_t val = iqs_read(tile, reg);
        if (val != IQS323_INVALID_RESPONSE)
            return val;
        tile->hal->delay_ms(5);
    }
    return IQS323_INVALID_RESPONSE;
}

/** Write with force-comms + verify readback.
 *  Each I2C STOP closes the window, so we need force_comms before
 *  both the write AND the readback. */
static void iqs_write_verified(tile_t *tile, uint8_t reg, uint16_t value)
{
    for (int attempt = 0; attempt < 10; attempt++) {
        iqs_force_comms(tile);
        iqs_write(tile, reg, value);
        iqs_force_comms(tile);
        uint16_t readback = iqs_read(tile, reg);
        if (readback == value)
            return;
    }
}

/** Read-modify-write with force-comms. */
static void iqs_modify(tile_t *tile, uint8_t reg, uint16_t mask, uint16_t val)
{
    uint16_t r = iqs_read_window(tile, reg);
    if (r == IQS323_INVALID_RESPONSE) return;
    r = (r & ~mask) | (val & mask);
    iqs_force_comms(tile);
    iqs_write(tile, reg, r);
}

/* ---- RDY pin EXTI callback ---- */

/* RDY pin EXTI callbacks — used when rdy_pin is configured.
 * Referenced by pointer in init(), so the compiler won't warn. */
static void _rdy_isr_0(void *ctx) { state[0].rdy_flag = 1; (void)ctx; }
static void _rdy_isr_1(void *ctx) { state[1].rdy_flag = 1; (void)ctx; }

/* ================================================================
 * Lifecycle
 * ================================================================ */

uint8_t tile_sense_t_c_find(tiles_pal_t *hal, uint8_t instance)
{
    uint8_t addr = resolve_id(instance);
    if (!addr) return 0;
    return hal->i2c_is_ready(hal->handle, addr) == 0;
}

void tile_sense_t_c_init(tiles_pal_t *hal, uint8_t instance,
                         tile_t *tile, const sense_t_c_cfg_t *cfg)
{
    memzero(tile, sizeof(tile_t));
    tile->hal = hal;
    tile->id  = resolve_id(instance);

    if (!tile->id) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_t_c: invalid instance");
        return;
    }

    /* Initialize per-instance state */
    iqs323_state_t *s = state_for(tile);
    memzero(s, sizeof(iqs323_state_t));
    if (cfg) {
        s->on_event = cfg->on_event;
        s->event_ctx = cfg->event_ctx;
        s->rdy_pin = cfg->rdy_pin;
        s->prox_threshold = cfg->prox_threshold;
        s->touch_threshold = cfg->touch_threshold;
    }

    /* Probe bus */
    if (hal->i2c_is_ready(hal->handle, tile->id) != 0) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_t_c: device not found");
        return;
    }

    /* Read Hardware ID */
    uint16_t hw_id = iqs_read_window(tile, IQS323_REG_HARDWARE_ID);
    tile->flags = (uint8_t)(hw_id & 0xFF);
    if (hw_id == IQS323_INVALID_RESPONSE) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_t_c: no communication window");
        return;
    }
    if (hw_id != IQS323_HW_ID_3DD && hw_id != IQS323_HW_ID_3ED) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_t_c: unexpected hardware ID");
        return;
    }

    /* Soft reset */
    iqs_force_comms(tile);
    iqs_write(tile, IQS323_REG_SYSTEM_CONTROL, IQS323_CTRL_SOFT_RESET);
    hal->delay_ms(20);

    /* ACK reset */
    iqs_force_comms(tile);
    iqs_write(tile, IQS323_REG_SYSTEM_CONTROL, IQS323_CTRL_ACK_RESET);
    hal->delay_ms(10);

    /* Disable unused channels if a channel mask was specified.
     * Sensor Setup bit 0 = Enable Channel. Clear it to disable. */
    uint8_t ch_mask = cfg ? cfg->channels : 0;
    if (ch_mask) {
        for (uint8_t ch = 0; ch < SENSE_T_C_NUM_CHANNELS; ch++) {
            if (!(ch_mask & (1 << ch))) {
                /* Disable this channel */
                uint16_t setup = iqs_read_window(tile, IQS323_REG_SENSOR_SETUP(ch));
                if (setup != IQS323_INVALID_RESPONSE) {
                    iqs_write_verified(tile, IQS323_REG_SENSOR_SETUP(ch), setup & ~0x01);
                }
            }
        }
    }

    /* Configure thresholds for enabled channels */
    if (s->prox_threshold) {
        uint16_t prox_val = (1U << 12) | (1U << 8) | s->prox_threshold;
        for (uint8_t ch = 0; ch < SENSE_T_C_NUM_CHANNELS; ch++) {
            if (!ch_mask || (ch_mask & (1 << ch)))
                iqs_write_verified(tile, IQS323_REG_PROX_SETTINGS(ch), prox_val);
        }
    }
    if (s->touch_threshold) {
        uint16_t touch_val = (2U << 8) | s->touch_threshold;
        for (uint8_t ch = 0; ch < SENSE_T_C_NUM_CHANNELS; ch++) {
            if (!ch_mask || (ch_mask & (1 << ch)))
                iqs_write_verified(tile, IQS323_REG_TOUCH_SETTINGS(ch), touch_val);
        }
    }

    /* Enable touch + prox events */
    iqs_write_verified(tile, IQS323_REG_EVENTS_ENABLE,
                       IQS323_STATUS_TOUCH_EVENT | IQS323_STATUS_PROX_EVENT);

    /* Set streaming mode + auto power */
    iqs_force_comms(tile);
    iqs_write(tile, IQS323_REG_SYSTEM_CONTROL,
              SENSE_T_C_POWER_AUTO << IQS323_CTRL_POWER_SHIFT);

    /* Trigger re-ATI and wait */
    iqs_force_comms(tile);
    iqs_write(tile, IQS323_REG_SYSTEM_CONTROL,
              (SENSE_T_C_POWER_AUTO << IQS323_CTRL_POWER_SHIFT) | IQS323_CTRL_RE_ATI);
    for (int i = 0; i < 50; i++) {
        hal->delay_ms(10);
        uint16_t st = iqs_read_window(tile, IQS323_REG_SYSTEM_STATUS);
        if (st != IQS323_INVALID_RESPONSE && !(st & IQS323_STATUS_ATI_ACTIVE))
            break;
    }

    /* Set up RDY pin interrupt if configured */
    s->rdy_flag = 0;
    s->last_status = 0;
    if (s->rdy_pin && hal->gpio_irq_enable) {
        void (*isr)(void *) = (instance == 0) ? _rdy_isr_0 : _rdy_isr_1;
        hal->gpio_irq_enable(hal->handle, s->rdy_pin,
                             TILES_GPIO_EDGE_FALLING, isr, NULL);
    }

    tile->state = TILE_STATE_READY;
}

/* ================================================================
 * Event processing
 * ================================================================ */

void tile_sense_t_c_process(tile_t *tile)
{
    if (tile->state != TILE_STATE_READY) return;

    iqs323_state_t *s = state_for(tile);

    /* In RDY mode, skip if no interrupt fired */
    if (s->rdy_pin && !s->rdy_flag)
        return;
    s->rdy_flag = 0;

    /* Read status. In RDY mode the window is already open (RDY went LOW),
     * so skip force_comms. In polled mode, force a window first. */
    if (!s->rdy_pin)
        iqs_force_comms(tile);
    uint16_t status = iqs_read(tile, IQS323_REG_SYSTEM_STATUS);
    if (status == IQS323_INVALID_RESPONSE)
        return;

    /* Fire callback on every valid read so LED tracks state in real time */
    if (s->on_event) {
        s->on_event(tile, status, s->event_ctx);
    }
    s->last_status = status;
}

void tile_sense_t_c_on_event(tile_t *tile, sense_t_c_event_cb_t cb, void *ctx)
{
    iqs323_state_t *s = state_for(tile);
    s->on_event = cb;
    s->event_ctx = ctx;
}

/* ================================================================
 * Lifecycle
 * ================================================================ */

void tile_sense_t_c_sleep(tile_t *tile)
{
    iqs_modify(tile, IQS323_REG_SYSTEM_CONTROL,
               IQS323_CTRL_POWER_MASK,
               SENSE_T_C_POWER_HALT << IQS323_CTRL_POWER_SHIFT);
    tile->state = TILE_STATE_SLEEPING;
}

void tile_sense_t_c_wake(tile_t *tile)
{
    iqs_modify(tile, IQS323_REG_SYSTEM_CONTROL,
               IQS323_CTRL_POWER_MASK,
               SENSE_T_C_POWER_NORMAL << IQS323_CTRL_POWER_SHIFT);
    tile->state = TILE_STATE_READY;
}

void tile_sense_t_c_reset(tile_t *tile)
{
    iqs_force_comms(tile);
    iqs_write(tile, IQS323_REG_SYSTEM_CONTROL, IQS323_CTRL_SOFT_RESET);
    tile->hal->delay_ms(20);
    tile->state = TILE_STATE_NONE;
}

/* ================================================================
 * Status (cached from last process() call)
 * ================================================================ */

uint16_t tile_sense_t_c_get_status(tile_t *tile)
{
    iqs323_state_t *s = state_for(tile);
    return s->last_status;
}

uint16_t tile_sense_t_c_get_gestures(tile_t *tile)
{
    return iqs_read_window(tile, IQS323_REG_GESTURE_STATUS);
}

uint8_t tile_sense_t_c_is_touched(tile_t *tile, uint8_t channel)
{
    if (channel >= SENSE_T_C_NUM_CHANNELS) return 0;
    iqs323_state_t *s = state_for(tile);
    return (s->last_status & IQS323_STATUS_CH_TOUCH(channel)) ? 1 : 0;
}

uint8_t tile_sense_t_c_is_prox(tile_t *tile, uint8_t channel)
{
    if (channel >= SENSE_T_C_NUM_CHANNELS) return 0;
    iqs323_state_t *s = state_for(tile);
    return (s->last_status & IQS323_STATUS_CH_PROX(channel)) ? 1 : 0;
}

/* ================================================================
 * Data
 * ================================================================ */

uint16_t tile_sense_t_c_get_counts(tile_t *tile, uint8_t channel)
{
    if (channel >= SENSE_T_C_NUM_CHANNELS) return 0;
    return iqs_read_window(tile, IQS323_REG_CH0_COUNTS + channel * 2);
}

uint16_t tile_sense_t_c_get_lta(tile_t *tile, uint8_t channel)
{
    if (channel >= SENSE_T_C_NUM_CHANNELS) return 0;
    return iqs_read_window(tile, IQS323_REG_CH0_LTA + channel * 2);
}

int16_t tile_sense_t_c_get_delta(tile_t *tile, uint8_t channel)
{
    uint16_t counts = tile_sense_t_c_get_counts(tile, channel);
    uint16_t lta    = tile_sense_t_c_get_lta(tile, channel);
    if (counts == IQS323_INVALID_RESPONSE || lta == IQS323_INVALID_RESPONSE)
        return 0;
    return (int16_t)(counts - lta);
}

uint16_t tile_sense_t_c_get_slider(tile_t *tile)
{
    return iqs_read_window(tile, IQS323_REG_SLIDER_POSITION);
}

/* ================================================================
 * Configuration
 * ================================================================ */

void tile_sense_t_c_set_thresholds(tile_t *tile, uint8_t channel,
                                   uint8_t prox_thresh, uint8_t touch_thresh)
{
    if (channel >= SENSE_T_C_NUM_CHANNELS) return;
    if (prox_thresh) {
        uint16_t val = (1U << 12) | (1U << 8) | prox_thresh;
        iqs_write_verified(tile, IQS323_REG_PROX_SETTINGS(channel), val);
    }
    if (touch_thresh) {
        uint16_t val = (2U << 8) | touch_thresh;
        iqs_write_verified(tile, IQS323_REG_TOUCH_SETTINGS(channel), val);
    }
}

void tile_sense_t_c_set_power_mode(tile_t *tile, sense_t_c_power_mode_t mode)
{
    iqs_modify(tile, IQS323_REG_SYSTEM_CONTROL,
               IQS323_CTRL_POWER_MASK,
               (uint16_t)mode << IQS323_CTRL_POWER_SHIFT);
}

void tile_sense_t_c_enable_events(tile_t *tile, uint16_t mask)
{
    iqs_write_verified(tile, IQS323_REG_EVENTS_ENABLE, mask);
}

void tile_sense_t_c_ati(tile_t *tile)
{
    iqs_force_comms(tile);
    uint16_t ctrl = iqs_read(tile, IQS323_REG_SYSTEM_CONTROL);
    if (ctrl == IQS323_INVALID_RESPONSE) ctrl = 0;
    iqs_force_comms(tile);
    iqs_write(tile, IQS323_REG_SYSTEM_CONTROL, ctrl | IQS323_CTRL_RE_ATI);
    for (int i = 0; i < 50; i++) {
        tile->hal->delay_ms(10);
        uint16_t st = iqs_read_window(tile, IQS323_REG_SYSTEM_STATUS);
        if (st != IQS323_INVALID_RESPONSE && !(st & IQS323_STATUS_ATI_ACTIVE))
            return;
    }
}

/* ================================================================
 * ATI fine-tuning + filter / conversion-frequency tuning
 * ================================================================ */

void tile_sense_t_c_set_ati_setup(tile_t *tile, uint8_t channel, uint16_t value)
{
    if (channel >= SENSE_T_C_NUM_CHANNELS) return;
    iqs_force_comms(tile);
    iqs_write(tile, IQS323_REG_ATI_SETUP(channel), value);
}

void tile_sense_t_c_set_counts_filter(tile_t *tile, uint16_t beta)
{
    iqs_force_comms(tile);
    iqs_write(tile, IQS323_REG_COUNTS_FILTER, beta);
}

void tile_sense_t_c_set_conversion_freq(tile_t *tile, uint8_t channel,
                                        uint16_t value)
{
    if (channel >= SENSE_T_C_NUM_CHANNELS) return;
    iqs_force_comms(tile);
    iqs_write(tile, IQS323_REG_CONV_FREQ(channel), value);
}

/* ================================================================
 * Reference UI — channel mode + reference sensor ID
 * ================================================================ */

void tile_sense_t_c_set_channel_mode(tile_t *tile, uint8_t channel,
                                     sense_t_c_channel_mode_t mode,
                                     uint8_t reference_id)
{
    if (channel >= SENSE_T_C_NUM_CHANNELS) return;

    /* Channel Setup register: bits [3:0] = Channel Mode,
     * bits [7:4] = Reference Sensor ID. Preserve the upper byte
     * (Follower Event Mask). */
    uint8_t reg = IQS323_REG_CH_SETUP(channel);
    iqs_force_comms(tile);
    uint16_t cur = iqs_read(tile, reg);
    if (cur == IQS323_INVALID_RESPONSE) cur = 0;

    uint16_t low = ((uint16_t)reference_id & 0x0F) << 4
                 | ((uint16_t)mode & 0x0F);
    uint16_t next = (cur & 0xFF00) | low;

    iqs_force_comms(tile);
    iqs_write(tile, reg, next);
}

/* ================================================================
 * I²C communication mode — event vs streaming
 * ================================================================ */

void tile_sense_t_c_set_comm_mode(tile_t *tile, sense_t_c_comm_mode_t mode)
{
    /* System Control bit 7 = Interface Selection
     *   0 = I²C streaming, 1 = I²C event. Preserve other bits. */
    iqs_force_comms(tile);
    uint16_t cur = iqs_read(tile, IQS323_REG_SYSTEM_CONTROL);
    if (cur == IQS323_INVALID_RESPONSE) cur = 0;
    uint16_t next = (mode == SENSE_T_C_COMM_EVENT)
                  ? (cur | (1U << 7))
                  : (cur & ~(1U << 7));
    iqs_force_comms(tile);
    iqs_write(tile, IQS323_REG_SYSTEM_CONTROL, next);
}

/* ================================================================
 * Tier-2 idiomatic helpers
 * ================================================================ */

/* Bitmask of all per-channel touch bits (CH0/CH1/CH2). */
#define IQS323_STATUS_ANY_TOUCH \
    (IQS323_STATUS_CH_TOUCH(0) | IQS323_STATUS_CH_TOUCH(1) | IQS323_STATUS_CH_TOUCH(2))

/* Bitmask covering every gesture event bit defined in §A.2. */
#define IQS323_GESTURE_ANY \
    (IQS323_GESTURE_TAP | IQS323_GESTURE_SWIPE_POS | IQS323_GESTURE_SWIPE_NEG | \
     IQS323_GESTURE_FLICK_POS | IQS323_GESTURE_FLICK_NEG | IQS323_GESTURE_HOLD)

/* Poll cadence for wait_for_* helpers (ms). 5ms keeps latency low while
 * leaving plenty of headroom for the host's other work; in RDY mode the
 * underlying process() returns immediately when no event is pending. */
#define SENSE_T_C_POLL_INTERVAL_MS  5

uint8_t tile_sense_t_c_is_touched_any(tile_t *tile)
{
    iqs323_state_t *s = state_for(tile);
    return (s->last_status & IQS323_STATUS_ANY_TOUCH) ? 1 : 0;
}

uint8_t tile_sense_t_c_wait_for_touch(tile_t *tile, uint32_t timeout_ms)
{
    if (tile->state != TILE_STATE_READY) return 0;

    uint32_t elapsed = 0;
    /* First sample: process() may already have a fresh status. */
    tile_sense_t_c_process(tile);
    if (tile_sense_t_c_is_touched_any(tile))
        return 1;

    while (elapsed < timeout_ms) {
        tile->hal->delay_ms(SENSE_T_C_POLL_INTERVAL_MS);
        elapsed += SENSE_T_C_POLL_INTERVAL_MS;
        tile_sense_t_c_process(tile);
        if (tile_sense_t_c_is_touched_any(tile))
            return 1;
    }
    return 0;
}

uint16_t tile_sense_t_c_wait_for_gesture(tile_t *tile, uint32_t timeout_ms)
{
    if (tile->state != TILE_STATE_READY) return 0;

    uint32_t elapsed = 0;
    /* Gesture status is read directly (not cached by process()) so we
     * always pull a fresh value here. */
    uint16_t g = iqs_read_window(tile, IQS323_REG_GESTURE_STATUS);
    if (g != IQS323_INVALID_RESPONSE && (g & IQS323_GESTURE_ANY))
        return g;

    while (elapsed < timeout_ms) {
        tile->hal->delay_ms(SENSE_T_C_POLL_INTERVAL_MS);
        elapsed += SENSE_T_C_POLL_INTERVAL_MS;
        g = iqs_read_window(tile, IQS323_REG_GESTURE_STATUS);
        if (g != IQS323_INVALID_RESPONSE && (g & IQS323_GESTURE_ANY))
            return g;
    }
    return 0;
}

uint8_t tile_sense_t_c_read_slider_pct(tile_t *tile, uint8_t *out_pct)
{
    if (!out_pct) return 0;

    uint16_t raw = iqs_read_window(tile, IQS323_REG_SLIDER_POSITION);
    if (raw == IQS323_INVALID_RESPONSE)
        return 0;

    /* Slider position is a 10-bit field (0..1023). Clamp defensively in
     * case a future firmware widens the range, then integer-scale to
     * 0..100. (raw * 100) fits in 17 bits when raw <= 1023, safely
     * inside uint32_t. */
    if (raw > 1023) raw = 1023;
    *out_pct = (uint8_t)(((uint32_t)raw * 100u) / 1023u);
    return 1;
}

/* ================================================================
 * Low-level register access
 * ================================================================ */

uint16_t tile_sense_t_c_read_reg(tile_t *tile, uint8_t reg)
{
    return iqs_read_window(tile, reg);
}

void tile_sense_t_c_write_reg(tile_t *tile, uint8_t reg, uint16_t value)
{
    iqs_force_comms(tile);
    iqs_write(tile, reg, value);
}
