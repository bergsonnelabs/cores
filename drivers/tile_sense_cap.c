/**
 * @file   tile_sense_cap.c
 * @brief  Sense.CAP (IQS7211A) -- platform-agnostic driver implementation.
 *
 * Platform-agnostic. All bus access via tile->hal function pointers.
 * Reference: Azoteq IQS7211A Datasheet v1.3, April 2025.
 *
 * Comms model: the device only serves registers inside a communication
 * window. In streaming mode it clock-stretches an early master and plain
 * reads work; in event or low-power modes, or with Comms Request enabled,
 * a read instead returns 0xEEEE and the master must ask for a window
 * (write 0x00 to 0xFF, wait, retry) — one window, one transaction. Every
 * access here retries behind that request, and nothing derived from an
 * 0xEEEE read is ever written back: 0xEEEE carries Comms Request, Event
 * Mode, Manual Control and WDT, so writing it strands the part in
 * request-only mode until it is power-cycled. Observed the hard way on a
 * Core.ST.L4.1 at 100 kHz, 2026-08-04.
 *
 * Register data is 16-bit little-endian, two bytes per 8-bit register
 * address, and reads auto-increment through consecutive addresses
 * (§11.5) — which is what lets process() pull the whole trackpad block
 * in one transaction.
 */

#include "tile_sense_cap.h"
#include <stddef.h>

/* ================================================================
 * Instance -> I2C address table
 * ================================================================ */

/* The IQS7211A address is fixed in silicon, so there is exactly one
 * instance per bus. The table keeps the shape of the other drivers. */
static const uint8_t id_table[] = {
    IQS7211A_I2C_ADDR,   /* 0: 0x56 */
};

#define NUM_INSTANCES  (sizeof(id_table) / sizeof(id_table[0]))

static uint8_t resolve_id(uint8_t instance)
{
    return (instance < NUM_INSTANCES) ? id_table[instance] : 0;
}

/* ================================================================
 * Per-instance driver state
 * ================================================================ */

/* Offsets into the cached data block, in registers from 0x10. */
#define BLK_INFO_FLAGS   0
#define BLK_GESTURES     1
#define BLK_RELATIVE_X   2
#define BLK_RELATIVE_Y   3
#define BLK_FINGER_BASE  4   /* finger n: base + n*4 = X, Y, strength, area */
#define BLK_WORDS        12  /* 0x10..0x1B */

typedef struct {
    sense_cap_event_cb_t on_event;
    void *event_ctx;
    uint16_t block[BLK_WORDS];     /* Cached 0x10..0x1B from last process() */
    uint16_t version_major;
    uint16_t version_minor;
    uint16_t config_shadow;        /* Last known-good Config Settings (0x51) */
    uint8_t  config_valid;         /* 1 once config_shadow holds a real read */
    volatile uint8_t rdy_flag;     /* Set by EXTI ISR */
    uint8_t rdy_pin;               /* Core pad number, 0 = polled */
} iqs7211a_state_t;

static iqs7211a_state_t state[NUM_INSTANCES];

static iqs7211a_state_t *state_for(tile_t *tile)
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

/** How many comms-request retries before giving a read up as failed. */
#define IQS_WINDOW_RETRIES  8
/** Settling time after a comms request, in ms (t_max is well under this). */
#define IQS_WINDOW_WAIT_MS  15

/**
 * Ask the device to open a communication window (§11.9.2): write 0x00 to
 * register 0xFF, then let it schedule the window. Also the terminate-comms
 * command in the other direction (§11.7) — same bytes, and harmless when a
 * window is already open.
 */
static void iqs_request_window(tile_t *tile)
{
    uint8_t zero = 0;
    tile->hal->i2c_write(tile->hal->handle, tile->id,
                         IQS7211A_REG_END_COMMS, &zero, 1);
    tile->hal->delay_ms(IQS_WINDOW_WAIT_MS);
}

/** Single raw read of a 16-bit little-endian register. */
static uint16_t iqs_read_once(tile_t *tile, uint8_t reg)
{
    uint8_t buf[2] = {0, 0};
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, buf, 2);
    return (uint16_t)(((uint16_t)buf[1] << 8) | buf[0]);
}

/**
 * Read a register, falling back to an explicit comms request.
 *
 * Returns IQS7211A_INVALID_RESPONSE if no window could be obtained —
 * callers must check before using the value, and must never write it
 * back.
 */
static uint16_t iqs_read(tile_t *tile, uint8_t reg)
{
    uint16_t v = iqs_read_once(tile, reg);
    for (uint8_t i = 0; v == IQS7211A_INVALID_RESPONSE && i < IQS_WINDOW_RETRIES; i++) {
        iqs_request_window(tile);
        v = iqs_read_once(tile, reg);
    }
    return v;
}

/** Write a 16-bit little-endian register. */
static void iqs_write(tile_t *tile, uint8_t reg, uint16_t value)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(value & 0xFF);
    buf[1] = (uint8_t)((value >> 8) & 0xFF);
    tile->hal->i2c_write(tile->hal->handle, tile->id, reg, buf, 2);
}

/**
 * Write a register and confirm it landed, requesting a window between
 * attempts. A write outside a window is silently dropped, so a blind
 * write is not enough.
 *
 * @return 1 if the readback matched, 0 if every attempt failed.
 */
static uint8_t iqs_write_verified(tile_t *tile, uint8_t reg, uint16_t value)
{
    for (uint8_t i = 0; i < IQS_WINDOW_RETRIES; i++) {
        /* Ask first, every time. One window serves one transaction, and
         * the STOP that ends this write closes it again — so the readback
         * below needs its own window, which iqs_read() arranges. */
        iqs_request_window(tile);
        iqs_write(tile, reg, value);

        if (iqs_read(tile, reg) == value)
            return 1;
    }
    return 0;
}

/**
 * Read-modify-write a 16-bit register.
 *
 * Bails out without writing when the read is invalid — writing a value
 * derived from 0xEEEE is what strands the device in request-only mode.
 */
static void iqs_modify(tile_t *tile, uint8_t reg, uint16_t clear, uint16_t set)
{
    uint16_t v = iqs_read(tile, reg);
    if (v == IQS7211A_INVALID_RESPONSE) {
        TILE_ON_ERROR(tile, "sense_cap: no comms window; register left alone");
        return;
    }
    v = (uint16_t)((v & (uint16_t)~clear) | set);
    iqs_write_verified(tile, reg, v);
}

/**
 * Pulse a System Control command bit.
 *
 * Every bit in System Control is a one-shot the device consumes, so only
 * the mode-select field has to survive the write. Deliberately does NOT
 * OR onto the whole read: carrying unknown high bits back in could set
 * Tx-test or a software reset, and an invalid read would set both.
 */
static void iqs_command(tile_t *tile, uint16_t bit)
{
    uint16_t ctrl = iqs_read(tile, IQS7211A_REG_SYSTEM_CONTROL);
    uint16_t mode = (ctrl == IQS7211A_INVALID_RESPONSE)
                        ? 0u
                        : (uint16_t)(ctrl & IQS7211A_CTRL_MODE_MASK);

    /* The read above ended in a STOP, which closed whatever window served
     * it, so this write needs a fresh one — unconditionally. Skipping it
     * is why an unverified command silently does nothing. Command bits
     * self-clear, so the caller must confirm the effect, not the value. */
    iqs_request_window(tile);
    iqs_write(tile, IQS7211A_REG_SYSTEM_CONTROL, (uint16_t)(mode | bit));
}

/**
 * Acknowledge the reset indication, and confirm it actually cleared.
 *
 * A command write outside a communication window is dropped without
 * complaint, so a single blind pulse is not enough — Show Reset simply
 * stays set and every later boot looks like an unexpected reset.
 *
 * @return 1 once Show Reset reads clear, 0 if it never did.
 */
static uint8_t iqs_ack_reset(tile_t *tile)
{
    for (uint8_t i = 0; i < IQS_WINDOW_RETRIES; i++) {
        iqs_command(tile, IQS7211A_CTRL_ACK_RESET);

        uint16_t info = iqs_read(tile, IQS7211A_REG_INFO_FLAGS);
        if (info != IQS7211A_INVALID_RESPONSE && !(info & IQS7211A_INFO_SHOW_RESET))
            return 1;

        iqs_request_window(tile);
    }
    return 0;
}

/** Cached info flags, or 0 when nothing has been read yet. */
static uint16_t cached_info(tile_t *tile)
{
    return state_for(tile)->block[BLK_INFO_FLAGS];
}

/* ---- RDY pin EXTI callback ---- */

/* Used only when cfg.rdy_pin is set; referenced by pointer in init(). */
static void _rdy_isr_0(void *ctx) { state[0].rdy_flag = 1; (void)ctx; }

/* ================================================================
 * Lifecycle
 * ================================================================ */

uint8_t tile_sense_cap_find(tiles_pal_t *hal, uint8_t instance)
{
    uint8_t addr = resolve_id(instance);
    if (!addr) return 0;
    return hal->i2c_is_ready(hal->handle, addr) == 0;
}

void tile_sense_cap_init(tiles_pal_t *hal, uint8_t instance,
                         tile_t *tile, const sense_cap_cfg_t *cfg)
{
    memzero(tile, sizeof(tile_t));
    tile->hal = hal;
    tile->id  = resolve_id(instance);

    if (!tile->id) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_cap: invalid instance");
        return;
    }

    iqs7211a_state_t *s = state_for(tile);
    memzero(s, sizeof(iqs7211a_state_t));
    if (cfg) {
        s->on_event  = cfg->on_event;
        s->event_ctx = cfg->event_ctx;
        s->rdy_pin   = cfg->rdy_pin;
    }

    /* Probe the bus */
    if (hal->i2c_is_ready(hal->handle, tile->id) != 0) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_cap: device not found");
        return;
    }
    tile->state = TILE_STATE_FOUND;

    /* Identify: the product number is the only fixed identity the part
     * offers — firmware versions vary between production batches. */
    uint16_t product = iqs_read(tile, IQS7211A_REG_PRODUCT_NUM);
    if (product == IQS7211A_INVALID_RESPONSE) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_cap: device ACKs but grants no comms window");
        return;
    }
    if (product != IQS7211A_PRODUCT_NUMBER) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_cap: wrong product number");
        return;
    }
    s->version_major = iqs_read(tile, IQS7211A_REG_MAJOR_VER);
    s->version_minor = iqs_read(tile, IQS7211A_REG_MINOR_VER);

    /* Snapshot Config Settings while the device is demonstrably talking.
     * sleep()/wake() restore from this instead of a live read-modify-write:
     * parking the part in manual-control LP2 makes windows scarce, and a
     * wake() that cannot read is a wake() that never returns the device. */
    uint16_t config = iqs_read(tile, IQS7211A_REG_CONFIG_SETTINGS);
    if (config != IQS7211A_INVALID_RESPONSE) {
        s->config_shadow = config;
        s->config_valid  = 1;
    }

    /* Clear the power-on reset indication so a later Show Reset means
     * something went wrong rather than "we just booted". Not fatal if it
     * fails — the device is otherwise fine, the flag is just stale. */
    if (!iqs_ack_reset(tile))
        TILE_ON_ERROR(tile, "sense_cap: reset indication would not clear");

    /* Apply only what the caller asked for. The device's trackpad
     * geometry and ATI settings are deliberately left alone — see the
     * `@studio unsupported` notes in the header. */
    if (cfg) {
        if (cfg->gestures)
            iqs_write_verified(tile, IQS7211A_REG_GESTURE_ENABLE, cfg->gestures);

        if (cfg->active_rate_ms)
            iqs_write_verified(tile, IQS7211A_REG_RATE_ACTIVE, cfg->active_rate_ms);

        if (cfg->max_touches) {
            /* 0x61 high byte = max multi-touches, low byte = total Txs.
             * Preserve the Tx count, which belongs to the geometry. */
            uint16_t v = iqs_read(tile, IQS7211A_REG_TP_TOUCHES);
            if (v != IQS7211A_INVALID_RESPONSE) {
                v = (uint16_t)((v & 0x00FF) | ((uint16_t)cfg->max_touches << 8));
                iqs_write_verified(tile, IQS7211A_REG_TP_TOUCHES, v);
            }
        }

        if (cfg->event_mode) {
            /* Event mode with no event source enabled would go silent,
             * so turn on the trackpad and gesture sources alongside it. */
            iqs_modify(tile, IQS7211A_REG_CONFIG_SETTINGS, 0,
                       (uint16_t)(IQS7211A_CFG_EVENT_MODE |
                                  IQS7211A_CFG_TP_EVENT |
                                  IQS7211A_CFG_GESTURE_EVENT));
        }
    }

    /* Set up the RDY interrupt if a pad map routes it. */
    s->rdy_flag = 0;
    if (s->rdy_pin && hal->gpio_irq_enable) {
        hal->gpio_irq_enable(hal->handle, s->rdy_pin,
                             TILES_GPIO_EDGE_FALLING, _rdy_isr_0, NULL);
    }

    tile->state = TILE_STATE_READY;
}

void tile_sense_cap_reset(tile_t *tile)
{
    if (tile->state == TILE_STATE_NONE) return;

    iqs_command(tile, IQS7211A_CTRL_SW_RESET);
    /* The reset lands once the comms window closes (§9.3.2); the part
     * then cold-boots. Give it time before touching the bus again. */
    tile->hal->delay_ms(100);

    iqs7211a_state_t *s = state_for(tile);
    memzero(s->block, sizeof(s->block));

    iqs_ack_reset(tile);
    tile->state = TILE_STATE_READY;
}

void tile_sense_cap_ack_reset(tile_t *tile)
{
    if (tile->state != TILE_STATE_READY && tile->state != TILE_STATE_SLEEPING)
        return;
    iqs_ack_reset(tile);
}

void tile_sense_cap_sleep(tile_t *tile)
{
    if (tile->state != TILE_STATE_READY) return;

    iqs7211a_state_t *s = state_for(tile);

    /* Without a trustworthy config snapshot there is no safe way back out
     * of manual control, so refuse rather than strand the device. */
    if (!s->config_valid) {
        TILE_ON_ERROR(tile, "sense_cap: no config snapshot; refusing to sleep");
        return;
    }

    /* Manual control hands mode switching to the host, then select LP2. */
    if (!iqs_write_verified(tile, IQS7211A_REG_CONFIG_SETTINGS,
                            (uint16_t)(s->config_shadow | IQS7211A_CFG_MANUAL_CONTROL))) {
        TILE_ON_ERROR(tile, "sense_cap: sleep aborted, config write failed");
        return;
    }
    iqs_write(tile, IQS7211A_REG_SYSTEM_CONTROL, (uint16_t)SENSE_CAP_MODE_LP2);
    tile->state = TILE_STATE_SLEEPING;
}

void tile_sense_cap_wake(tile_t *tile)
{
    if (tile->state != TILE_STATE_SLEEPING && tile->state != TILE_STATE_READY)
        return;

    iqs7211a_state_t *s = state_for(tile);

    /* Mode select first, and blind: in LP2 a read may well fail, and this
     * write is a plain assignment — every other bit in System Control is a
     * self-clearing one-shot, so there is nothing to preserve. */
    iqs_request_window(tile);
    iqs_write(tile, IQS7211A_REG_SYSTEM_CONTROL, (uint16_t)SENSE_CAP_MODE_ACTIVE);

    /* Hand mode switching back to the device's own timers, from the
     * snapshot rather than a read that may not land. */
    if (s->config_valid) {
        iqs_write_verified(tile, IQS7211A_REG_CONFIG_SETTINGS,
                           (uint16_t)(s->config_shadow & (uint16_t)~IQS7211A_CFG_MANUAL_CONTROL));
    }
    tile->state = TILE_STATE_READY;
}

/* ================================================================
 * Event processing
 * ================================================================ */

void tile_sense_cap_process(tile_t *tile)
{
    if (tile->state != TILE_STATE_READY) return;

    iqs7211a_state_t *s = state_for(tile);

    /* In RDY mode, do nothing until the device says it has data. */
    if (s->rdy_pin && !s->rdy_flag)
        return;
    s->rdy_flag = 0;

    /* One burst across 0x10..0x1B — reads auto-increment (§11.5). */
    uint8_t buf[BLK_WORDS * 2];
    uint16_t info = IQS7211A_INVALID_RESPONSE;

    for (uint8_t attempt = 0; attempt < IQS_WINDOW_RETRIES; attempt++) {
        memzero(buf, sizeof(buf));
        tile->hal->i2c_read(tile->hal->handle, tile->id,
                            IQS7211A_REG_INFO_FLAGS, buf, sizeof(buf));
        info = (uint16_t)(((uint16_t)buf[1] << 8) | buf[0]);
        if (info != IQS7211A_INVALID_RESPONSE)
            break;
        iqs_request_window(tile);
    }

    /* No window: keep the previous snapshot rather than caching garbage,
     * and stay quiet — the next cycle usually lands. */
    if (info == IQS7211A_INVALID_RESPONSE)
        return;

    for (uint8_t i = 0; i < BLK_WORDS; i++)
        s->block[i] = (uint16_t)(((uint16_t)buf[i * 2 + 1] << 8) | buf[i * 2]);

    if (s->on_event)
        s->on_event(tile, s->block[BLK_INFO_FLAGS], s->event_ctx);
}

void tile_sense_cap_on_event(tile_t *tile, sense_cap_event_cb_t cb, void *ctx)
{
    iqs7211a_state_t *s = state_for(tile);
    s->on_event  = cb;
    s->event_ctx = ctx;
}

/* ================================================================
 * Identification
 * ================================================================ */

uint16_t tile_sense_cap_get_version_major(tile_t *tile)
{
    return state_for(tile)->version_major;
}

uint16_t tile_sense_cap_get_version_minor(tile_t *tile)
{
    return state_for(tile)->version_minor;
}

uint16_t tile_sense_cap_get_settings_version(tile_t *tile)
{
    if (tile->state != TILE_STATE_READY) return 0;
    uint16_t v = iqs_read(tile, IQS7211A_REG_SETTINGS_VER);
    return (v == IQS7211A_INVALID_RESPONSE) ? 0 : v;
}

/* ================================================================
 * Runtime data
 * ================================================================ */

uint16_t tile_sense_cap_get_info_flags(tile_t *tile)
{
    return cached_info(tile);
}

uint16_t tile_sense_cap_get_gestures(tile_t *tile)
{
    return state_for(tile)->block[BLK_GESTURES];
}

uint8_t tile_sense_cap_get_num_fingers(tile_t *tile)
{
    uint16_t info = cached_info(tile);
    return (uint8_t)((info & IQS7211A_INFO_NUM_FINGERS_MASK)
                     >> IQS7211A_INFO_NUM_FINGERS_SHIFT);
}

/** Fetch one word of a finger's 4-word slot. */
static uint16_t finger_word(tile_t *tile, uint8_t finger, uint8_t word)
{
    if (finger >= SENSE_CAP_NUM_FINGERS) return 0;
    return state_for(tile)->block[BLK_FINGER_BASE + finger * 4 + word];
}

uint16_t tile_sense_cap_get_finger_x(tile_t *tile, uint8_t finger)
{
    return finger_word(tile, finger, 0);
}

uint16_t tile_sense_cap_get_finger_y(tile_t *tile, uint8_t finger)
{
    return finger_word(tile, finger, 1);
}

uint16_t tile_sense_cap_get_finger_strength(tile_t *tile, uint8_t finger)
{
    return finger_word(tile, finger, 2);
}

uint16_t tile_sense_cap_get_finger_area(tile_t *tile, uint8_t finger)
{
    return finger_word(tile, finger, 3);
}

int16_t tile_sense_cap_get_relative_x(tile_t *tile)
{
    return (int16_t)state_for(tile)->block[BLK_RELATIVE_X];
}

int16_t tile_sense_cap_get_relative_y(tile_t *tile)
{
    return (int16_t)state_for(tile)->block[BLK_RELATIVE_Y];
}

uint32_t tile_sense_cap_get_touch_status(tile_t *tile)
{
    if (tile->state != TILE_STATE_READY) return 0;

    uint8_t buf[4] = {0, 0, 0, 0};
    uint32_t low = IQS7211A_INVALID_RESPONSE;

    for (uint8_t attempt = 0; attempt < IQS_WINDOW_RETRIES; attempt++) {
        tile->hal->i2c_read(tile->hal->handle, tile->id,
                            IQS7211A_REG_TOUCH_STATUS_0, buf, sizeof(buf));
        low = (uint32_t)(((uint16_t)buf[1] << 8) | buf[0]);
        if (low != IQS7211A_INVALID_RESPONSE)
            break;
        iqs_request_window(tile);
    }
    if (low == IQS7211A_INVALID_RESPONSE)
        return 0;

    uint32_t high = (uint32_t)(((uint16_t)buf[3] << 8) | buf[2]);
    return (high << 16) | low;
}

uint8_t tile_sense_cap_is_touched(tile_t *tile, uint8_t channel)
{
    if (channel > 31) return 0;
    return (tile_sense_cap_get_touch_status(tile) >> channel) & 1U;
}

uint8_t tile_sense_cap_is_alp_active(tile_t *tile)
{
    return (cached_info(tile) & IQS7211A_INFO_ALP_OUTPUT) ? 1 : 0;
}

uint16_t tile_sense_cap_get_alp_count(tile_t *tile)
{
    if (tile->state != TILE_STATE_READY) return 0;
    uint16_t v = iqs_read(tile, IQS7211A_REG_ALP_COUNT);
    return (v == IQS7211A_INVALID_RESPONSE) ? 0 : v;
}

uint16_t tile_sense_cap_get_alp_lta(tile_t *tile)
{
    if (tile->state != TILE_STATE_READY) return 0;
    uint16_t v = iqs_read(tile, IQS7211A_REG_ALP_LTA);
    return (v == IQS7211A_INVALID_RESPONSE) ? 0 : v;
}

uint8_t tile_sense_cap_get_mode(tile_t *tile)
{
    return (uint8_t)((cached_info(tile) & IQS7211A_INFO_MODE_MASK)
                     >> IQS7211A_INFO_MODE_SHIFT);
}

uint8_t tile_sense_cap_has_ati_error(tile_t *tile)
{
    uint16_t info = cached_info(tile);
    return (info & (IQS7211A_INFO_ATI_ERROR | IQS7211A_INFO_ALP_ATI_ERROR)) ? 1 : 0;
}

/* ================================================================
 * Configuration
 * ================================================================ */

void tile_sense_cap_enable_gestures(tile_t *tile, uint16_t mask)
{
    if (tile->state != TILE_STATE_READY) return;
    iqs_write(tile, IQS7211A_REG_GESTURE_ENABLE, mask);
}

void tile_sense_cap_set_tap_timing(tile_t *tile, uint16_t tap_ms, uint16_t hold_ms)
{
    if (tile->state != TILE_STATE_READY) return;
    iqs_write(tile, IQS7211A_REG_TAP_TIME,  tap_ms);
    iqs_write(tile, IQS7211A_REG_HOLD_TIME, hold_ms);
}

void tile_sense_cap_set_swipe_timing(tile_t *tile, uint16_t swipe_ms,
                                     uint16_t x_dist, uint16_t y_dist)
{
    if (tile->state != TILE_STATE_READY) return;
    iqs_write(tile, IQS7211A_REG_SWIPE_TIME,   swipe_ms);
    iqs_write(tile, IQS7211A_REG_SWIPE_X_DIST, x_dist);
    iqs_write(tile, IQS7211A_REG_SWIPE_Y_DIST, y_dist);
}

/** Map a mode onto its report-rate register, or 0 if there isn't one. */
static uint8_t rate_reg_for_mode(uint8_t mode)
{
    switch (mode) {
    case SENSE_CAP_MODE_ACTIVE:     return IQS7211A_REG_RATE_ACTIVE;
    case SENSE_CAP_MODE_IDLE_TOUCH: return IQS7211A_REG_RATE_IDLE_TOUCH;
    case SENSE_CAP_MODE_IDLE:       return IQS7211A_REG_RATE_IDLE;
    case SENSE_CAP_MODE_LP1:        return IQS7211A_REG_RATE_LP1;
    case SENSE_CAP_MODE_LP2:        return IQS7211A_REG_RATE_LP2;
    default:                        return 0;
    }
}

void tile_sense_cap_set_report_rate(tile_t *tile, uint8_t mode, uint16_t ms)
{
    if (tile->state != TILE_STATE_READY) return;
    uint8_t reg = rate_reg_for_mode(mode);
    if (!reg) return;
    iqs_write(tile, reg, ms);
}

/** Map a mode onto its timeout register. LP2 has no timeout — it is the floor. */
static uint8_t timeout_reg_for_mode(uint8_t mode)
{
    switch (mode) {
    case SENSE_CAP_MODE_ACTIVE:     return IQS7211A_REG_TIMEOUT_ACTIVE;
    case SENSE_CAP_MODE_IDLE_TOUCH: return IQS7211A_REG_TIMEOUT_IDLE_TOUCH;
    case SENSE_CAP_MODE_IDLE:       return IQS7211A_REG_TIMEOUT_IDLE;
    case SENSE_CAP_MODE_LP1:        return IQS7211A_REG_TIMEOUT_LP1;
    default:                        return 0;
    }
}

void tile_sense_cap_set_mode_timeout(tile_t *tile, uint8_t mode, uint16_t seconds)
{
    if (tile->state != TILE_STATE_READY) return;
    uint8_t reg = timeout_reg_for_mode(mode);
    if (!reg) return;
    iqs_write(tile, reg, seconds);
}

void tile_sense_cap_set_max_touches(tile_t *tile, uint8_t fingers)
{
    if (tile->state != TILE_STATE_READY) return;
    if (fingers < 1 || fingers > SENSE_CAP_NUM_FINGERS) return;

    /* High byte only — the low byte is the total Tx count (geometry). */
    uint16_t v = iqs_read(tile, IQS7211A_REG_TP_TOUCHES);
    v = (uint16_t)((v & 0x00FF) | ((uint16_t)fingers << 8));
    iqs_write(tile, IQS7211A_REG_TP_TOUCHES, v);
}

void tile_sense_cap_set_resolution(tile_t *tile, uint16_t x_res, uint16_t y_res)
{
    if (tile->state != TILE_STATE_READY) return;
    iqs_write(tile, IQS7211A_REG_X_RESOLUTION, x_res);
    iqs_write(tile, IQS7211A_REG_Y_RESOLUTION, y_res);
}

void tile_sense_cap_set_event_mode(tile_t *tile, uint8_t enable, uint16_t events)
{
    if (tile->state != TILE_STATE_READY) return;

    uint16_t clear = IQS7211A_CFG_EVENT_MODE;
    uint16_t set   = enable ? IQS7211A_CFG_EVENT_MODE : 0;

    if (events) {
        clear |= IQS7211A_CFG_EVENT_MASK;
        set   |= (uint16_t)(events & IQS7211A_CFG_EVENT_MASK);
    }
    iqs_modify(tile, IQS7211A_REG_CONFIG_SETTINGS, clear, set);
}

void tile_sense_cap_set_watchdog(tile_t *tile, uint8_t enable)
{
    if (tile->state != TILE_STATE_READY) return;
    iqs_modify(tile, IQS7211A_REG_CONFIG_SETTINGS,
               IQS7211A_CFG_WDT_EN, enable ? IQS7211A_CFG_WDT_EN : 0);
}

uint8_t tile_sense_cap_re_ati(tile_t *tile)
{
    if (tile->state != TILE_STATE_READY) return 0;

    /* Clear any stale completion flags, then queue both channels. */
    iqs_read(tile, IQS7211A_REG_INFO_FLAGS);
    iqs_command(tile, (uint16_t)(IQS7211A_CTRL_TP_RE_ATI | IQS7211A_CTRL_ALP_RE_ATI));

    /* ATI runs when the channels are next sensed, so wait on the
     * completion flags rather than a fixed delay. */
    for (uint8_t i = 0; i < 50; i++) {
        tile->hal->delay_ms(10);
        uint16_t info = iqs_read(tile, IQS7211A_REG_INFO_FLAGS);

        if (info & (IQS7211A_INFO_ATI_ERROR | IQS7211A_INFO_ALP_ATI_ERROR))
            return 0;
        if (info & IQS7211A_INFO_RE_ATI_OCCURRED)
            return 1;
    }
    return 0;   /* timed out */
}

void tile_sense_cap_reseed(tile_t *tile)
{
    if (tile->state != TILE_STATE_READY) return;
    iqs_command(tile, (uint16_t)(IQS7211A_CTRL_TP_RESEED | IQS7211A_CTRL_ALP_RESEED));
}

/* ================================================================
 * Advanced / escape hatch
 * ================================================================ */

uint16_t tile_sense_cap_read_reg(tile_t *tile, uint8_t reg)
{
    if (tile->state != TILE_STATE_READY && tile->state != TILE_STATE_FOUND)
        return 0;
    /* Returns IQS7211A_INVALID_RESPONSE verbatim when no window opened —
     * callers at this level want to see that, not a laundered zero. */
    return iqs_read(tile, reg);
}

void tile_sense_cap_write_reg(tile_t *tile, uint8_t reg, uint16_t value)
{
    if (tile->state != TILE_STATE_READY && tile->state != TILE_STATE_FOUND)
        return;
    iqs_write_verified(tile, reg, value);
}
