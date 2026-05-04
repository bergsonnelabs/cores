/**
 * @file   tile_power_l_1t.h
 * @brief  Li-Ion charge controller driver for the Power.L.1T tile (rev a).
 *
 * Embeds the Texas Instruments BQ25150, a single-cell Li-Ion charge
 * controller with programmable 1.8 V LDO output and 12-bit ADC for
 * battery and system monitoring.
 *
 * Key specifications:
 *   - Charge input:     3.4–5.5 V (up to 500 mA)
 *   - Battery voltage:  3.6–4.6 V (programmable in 10 mV steps)
 *   - LDO output:       1.8 V on this tile (chip is programmable)
 *   - ADC:              12-bit, 6 input channels
 *   - Charge current:   1.25–500 mA programmable
 *   - JEITA-style NTC:  cold / cool / warm / hot thresholds
 *
 * Datasheet: https://www.bergsonne.io/tiles/power/l1t
 * IC datasheet: https://www.ti.com/lit/ds/symlink/bq25150.pdf
 *
 * @studio tile label=Power.L.1T icon=☷
 *
 * Quick start:
 * @code
 *   #include "core_tiles.h"
 *
 *   tile_t battery;
 *   tile_power_l_1t_init(core_tiles_pal(&core_i2c1), 0, &battery, NULL);
 *   if (tile_is_ready(&battery)) {
 *       tile_power_l_1t_set_charge_current_ma(&battery, 100);
 *       tile_power_l_1t_set_charge_voltage_mv(&battery, 4200);
 *       uint16_t vbat = tile_power_l_1t_get_vbat_mv(&battery);
 *       if (tile_power_l_1t_is_battery_low(&battery, 10)) {
 *           // go to sleep, save the cell
 *       }
 *   }
 * @endcode
 *
 * Driver gaps (chip capabilities not exposed by this driver):
 *
 * @studio unsupported severity=advanced category="MR button + INT pin handling" section=advanced
 *   The chip's MR (push-button) and INT (interrupt) pins aren't
 *   routed to tile pads on the current revision — nothing for a
 *   Core GPIO to attach to. Closing this gap requires a tile
 *   hardware revision that routes at least INT to a connector
 *   pad. Until then, MASK0–3 / MRCTRL register writes have no
 *   external effect.
 */

#ifndef INC_TILE_POWER_L_1T_H_
#define INC_TILE_POWER_L_1T_H_

#include "tiles.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* Driver version                                                  */
/* -------------------------------------------------------------- */

#define TILE_POWER_L_1T_VERSION_MAJOR  3
#define TILE_POWER_L_1T_VERSION_MINOR  1
#define TILE_POWER_L_1T_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);  /* requires tiles.h >= 1.0 */

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

/**
 * @brief  Instance-to-address mapping for Power.L.1T.
 *
 * | Instance | ID   | Bus  | Hardware config      |
 * |----------|------|------|----------------------|
 * | 0        | 0x6B | I2C  | Fixed address        |
 *
 * @note  The BQ25150 has a single fixed I2C address. Multiple
 *        Power.L.1T tiles require separate I2C buses.
 */
#define BQ25150_I2C_ADDR_DEFAULT    0x6B

/* -------------------------------------------------------------- */
/* BQ25150 register map (subset used by this driver)               */
/* -------------------------------------------------------------- */

#define BQ25150_REG_STAT0           0x00  /**< Charger Status 0 */
#define BQ25150_REG_STAT1           0x01  /**< Charger Status 1 */
#define BQ25150_REG_STAT2           0x02  /**< ADC Status */
#define BQ25150_REG_FLAG0           0x03  /**< Charger Flags 0 */
#define BQ25150_REG_FLAG1           0x04  /**< Charger Flags 1 */
#define BQ25150_REG_FLAG2           0x05  /**< ADC Flags */
#define BQ25150_REG_FLAG3           0x06  /**< Timer Flags */
#define BQ25150_REG_VBAT_CTRL       0x12  /**< Battery Voltage Control */
#define BQ25150_REG_ICHG_CTRL       0x13  /**< Fast Charge Current Control */
#define BQ25150_REG_PCHRGCTRL       0x14  /**< Pre-Charge Current Control */
#define BQ25150_REG_TERMCTRL        0x15  /**< Termination Current Control */
#define BQ25150_REG_BUVLO           0x16  /**< Battery UVLO and Current Limit */
#define BQ25150_REG_CHARGERCTRL0    0x17  /**< Charger Control 0 (TS, watchdog) */
#define BQ25150_REG_ILIMCTRL        0x19  /**< Input Current Limit Control */
#define BQ25150_REG_LDOCTRL         0x1D  /**< LDO Control (enable, voltage, mode) */
#define BQ25150_REG_ICCTRL0         0x35  /**< IC Control 0 (ship mode) */
#define BQ25150_REG_ADCCTRL0        0x40  /**< ADC Control 0 */
#define BQ25150_REG_ADC_DATA_VBAT_M 0x42  /**< ADC VBAT MSB */
#define BQ25150_REG_ADC_DATA_VBAT_L 0x43  /**< ADC VBAT LSB */
#define BQ25150_REG_ADC_DATA_TS_M   0x44  /**< ADC TS MSB */
#define BQ25150_REG_ADC_DATA_ICHG_M 0x46  /**< ADC ICHG MSB */
#define BQ25150_REG_ADC_DATA_ADCIN_M 0x48 /**< ADC ADCIN MSB */
#define BQ25150_REG_ADC_DATA_VIN_M  0x4A  /**< ADC VIN MSB */
#define BQ25150_REG_ADC_DATA_PMID_M 0x4C  /**< ADC PMID MSB */
#define BQ25150_REG_ADC_DATA_IIN_M  0x4E  /**< ADC IIN MSB */
#define BQ25150_REG_ADC_READ_EN     0x58  /**< ADC Channel Enable */
#define BQ25150_REG_TS_FASTCHGCTRL  0x61  /**< TS Charge Control */
#define BQ25150_REG_TS_COLD         0x62  /**< TS Cold Threshold */
#define BQ25150_REG_TS_COOL         0x63  /**< TS Cool Threshold */
#define BQ25150_REG_TS_WARM         0x64  /**< TS Warm Threshold */
#define BQ25150_REG_TS_HOT          0x65  /**< TS Hot Threshold */
#define BQ25150_REG_DEVICE_ID       0x6F  /**< Device identification */

/** @brief  Expected DEVICE_ID register value. */
#define BQ25150_DEVICE_ID_DEFAULT   0x20

/* -------------------------------------------------------------- */
/* Status struct                                                   */
/* -------------------------------------------------------------- */

/**
 * @brief  Snapshot of the BQ25150's current state and latched faults.
 *
 * Populated by tile_power_l_1t_get_charge_status(). Mixes live state
 * (vin_pgood, charging, *_active) and clear-on-read event flags
 * (faults). Reading the flags clears them — call once per polling
 * cycle, not multiple times.
 */
typedef struct {
    /* Live state (from STAT0 / STAT1) */
    uint16_t vin_pgood       : 1;  /**< 1 = VIN within valid range */
    uint16_t charging        : 1;  /**< 1 = charging active */
    uint16_t cv_mode         : 1;  /**< 1 = CV (taper) phase */
    uint16_t charge_done     : 1;  /**< 1 = termination reached */
    uint16_t iinlim_active   : 1;  /**< 1 = input current limit reducing charge */
    uint16_t vindpm_active   : 1;  /**< 1 = VIN dynamic power management active */
    uint16_t thermreg_active : 1;  /**< 1 = thermal regulation foldback active */
    /* Faults (from FLAG1 / FLAG3 — clear on read) */
    uint16_t vin_ovp         : 1;  /**< VIN above OVP threshold */
    uint16_t bat_ocp         : 1;  /**< Battery over-current detected */
    uint16_t bat_uvlo        : 1;  /**< Battery under-voltage lockout */
    uint16_t safety_timer    : 1;  /**< Charge safety timer expired */
    uint16_t watchdog        : 1;  /**< I²C watchdog expired (init disables WD, so should be 0) */
    /* TS thresholds (from STAT1 / FLAG1) */
    uint16_t ts_cold         : 1;  /**< NTC reads below TS_COLD (charging paused) */
    uint16_t ts_cool         : 1;  /**< NTC in cool band (reduced charge current) */
    uint16_t ts_warm         : 1;  /**< NTC in warm band (reduced charge voltage) */
    uint16_t ts_hot          : 1;  /**< NTC reads above TS_HOT (charging stopped) */
} power_l_1t_status_t;

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

/**
 * @brief  Check whether a BQ25150 is present on the I2C bus.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @return 1 if device ACKs, 0 otherwise
 */
uint8_t tile_power_l_1t_find(tiles_pal_t* hal, uint8_t instance);

/**
 * Optional init config. Pass NULL for defaults.
 * Reserved for future use.
 */
typedef struct {
    uint8_t reserved;   /**< Placeholder — no options yet. */
} power_l_1t_cfg_t;

/**
 * @brief  Initialize the BQ25150 charge controller.
 *
 * Verifies the device ID, disables the I²C watchdog (the chip
 * otherwise resets all charge parameters every 50 s), enables all
 * 6 ADC channels, configures sane charge defaults, and exits ship
 * mode. Pass cfg=NULL for defaults.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @param  tile      Pointer to tile handle (populated by this function)
 * @param  cfg       Optional config, or NULL for defaults
 */
void tile_power_l_1t_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                          const power_l_1t_cfg_t *cfg);

/* ---- Charge configuration ---------------------------------------- */

/**
 * @brief  Set the fast-charge current.
 *
 * Programs ICHG_CTRL with the appropriate ICHARGE_RANGE bit so the
 * resolution scales: ≤318 mA uses 1.25 mA steps, >318 mA uses
 * 2.5 mA steps. Values clamp to 1.25–500 mA.
 *
 * @studio expose category=tile name=set_charge_current_ma section=config
 * @param  tile  Initialised tile handle
 * @param  ma    Target charge current in mA (1.25–500)
 */
void tile_power_l_1t_set_charge_current_ma(tile_t* tile, uint16_t ma);

/**
 * @brief  Set the battery regulation voltage.
 *
 * VBATREG = 3600 + code × 10 mV. Values outside 3600–4600 mV clamp
 * to that range. Use 4200 for typical Li-Ion, 3650 for LFP, 4350+
 * for high-voltage NMC variants.
 *
 * @studio expose category=tile name=set_charge_voltage_mv section=config
 * @param  tile  Initialised tile handle
 * @param  mv    Target battery voltage in mV (3600–4600)
 */
void tile_power_l_1t_set_charge_voltage_mv(tile_t* tile, uint16_t mv);

/**
 * @brief  Set the pre-charge current (used when VBAT < VLOW).
 *
 * Pre-charge applies to deeply-discharged cells; usually 10–20 % of
 * the fast-charge rate. Values clamp to 1.25–77.5 mA.
 *
 * @studio expose category=tile name=set_pre_charge_ma section=config
 * @param  tile  Initialised tile handle
 * @param  ma   Target pre-charge current in mA (1.25–77.5)
 */
void tile_power_l_1t_set_pre_charge_ma(tile_t* tile, uint8_t ma);

/**
 * @brief  Set the termination current as a percentage of ICHG.
 *
 * Charging ends when the cell's current draw drops below this
 * threshold during the CV phase. 0 disables termination (charges
 * continuously until VBATREG is reached). Values clamp to 1–31 %.
 *
 * @studio expose category=tile name=set_termination_percent section=config
 * @param  tile  Initialised tile handle
 * @param  pct   Termination current as % of ICHG (0 = disabled, 1–31)
 */
void tile_power_l_1t_set_termination_percent(tile_t* tile, uint8_t pct);

/**
 * @brief  Set the input current limit (DPM threshold).
 *
 * When the adapter can't supply more than this, charge current
 * folds back to keep VIN above the DPM threshold. Values clamp to
 * 5–500 mA in 5 mA steps.
 *
 * @studio expose category=tile name=set_input_current_limit_ma section=config
 * @param  tile  Initialised tile handle
 * @param  ma   Input current limit in mA (5–500)
 */
void tile_power_l_1t_set_input_current_limit_ma(tile_t* tile, uint16_t ma);

/* ---- ADC reads (milli-units) ------------------------------------- */

/**
 * @brief  Read battery voltage from the ADC.
 *
 * Replaces the deprecated raw-counts API; performs a 2-byte burst
 * read to avoid torn samples.
 *
 * @studio expose category=tile name=get_vbat_mv returns=int section=runtime
 * @param  tile  Initialised tile handle
 * @return Battery voltage in mV (0–6000)
 */
uint16_t tile_power_l_1t_get_vbat_mv(tile_t* tile);

/**
 * @brief  Read input (VIN) voltage from the ADC.
 * @studio expose category=tile name=get_vin_mv returns=int section=runtime
 * @param  tile  Initialised tile handle
 * @return VIN in mV (0–6000)
 */
uint16_t tile_power_l_1t_get_vin_mv(tile_t* tile);

/**
 * @brief  Read PMID (system rail) voltage from the ADC.
 * @studio expose category=tile name=get_pmid_mv returns=int section=runtime
 * @param  tile  Initialised tile handle
 * @return PMID in mV (0–6000)
 */
uint16_t tile_power_l_1t_get_pmid_mv(tile_t* tile);

/**
 * @brief  Read charge current (into the battery) from the ADC.
 *
 * Scaled relative to the configured ICHG fast-charge limit; returns
 * actual mA flowing into the cell.
 *
 * @studio expose category=tile name=get_charge_current_ma returns=int section=runtime
 * @param  tile  Initialised tile handle
 * @return Charge current in mA (0–500)
 */
uint16_t tile_power_l_1t_get_charge_current_ma(tile_t* tile);

/**
 * @brief  Read input current (from VIN) from the ADC.
 *
 * Range scales with ILIMCTRL: ≤150 mA range gives 0–375 mA full
 * scale; >150 mA range gives 0–750 mA full scale.
 *
 * @studio expose category=tile name=get_input_current_ma returns=int section=runtime
 * @param  tile  Initialised tile handle
 * @return Input current in mA (0–750)
 */
uint16_t tile_power_l_1t_get_input_current_ma(tile_t* tile);

/**
 * @brief  Read raw TS pin voltage from the ADC.
 *
 * Returns the millivolt reading on the TS pin (0–1200 mV). Convert
 * to °C using the NTC's resistance/temperature curve in firmware —
 * the chip doesn't expose temperature directly.
 *
 * @studio expose category=tile name=get_ts_mv returns=int section=runtime
 * @param  tile  Initialised tile handle
 * @return TS voltage in mV (0–1200)
 */
uint16_t tile_power_l_1t_get_ts_mv(tile_t* tile);

/**
 * @brief  Read raw ADCIN pin voltage from the ADC.
 *
 * Optional auxiliary input (0–1.2 V range). Useful for monitoring
 * an external sensor (e.g., separate NTC, voltage divider).
 *
 * @studio expose category=tile name=get_adcin_mv returns=int section=runtime
 * @param  tile  Initialised tile handle
 * @return ADCIN voltage in mV (0–1200)
 */
uint16_t tile_power_l_1t_get_adcin_mv(tile_t* tile);

/* ---- Battery percent (derived) ---------------------------------- */

/**
 * @brief  Read battery state-of-charge as a percentage.
 *
 * Derived from VBAT using a linear curve (3000 mV = 0%, 4200 mV =
 * 100%). Coarse — a real fuel gauge would integrate Coulombs.
 *
 * @studio expose category=tile name=get_percent returns=int section=runtime
 * @param  tile  Initialised tile handle
 * @return Battery percentage (0–100)
 */
uint8_t tile_power_l_1t_get_percent(tile_t* tile);

/* ---- NTC (TS pin) thresholds ------------------------------------- */

/**
 * @brief  Set the cold-temperature TS threshold (raw register value).
 *
 * The TS register codes are bit-positional (1, 2, 4, 8, 16, ...
 * encode multiples of 4.688 mV up to 600 mV). Datasheet section
 * 8.5.1.49–52 describes the exact mapping; users tuning JEITA
 * thresholds should consult the chip's NTC profile guide.
 *
 * @studio expose category=tile name=set_ts_cold section=config
 * @param  tile  Initialised tile handle
 * @param  code  Raw 8-bit TS_COLD register value
 */
void tile_power_l_1t_set_ts_cold(tile_t* tile, uint8_t code);

/**
 * @brief  Set the cool-temperature TS threshold (raw register value).
 * @studio expose category=tile name=set_ts_cool section=config
 * @param  tile  Initialised tile handle
 * @param  code  Raw 8-bit TS_COOL register value
 */
void tile_power_l_1t_set_ts_cool(tile_t* tile, uint8_t code);

/**
 * @brief  Set the warm-temperature TS threshold (raw register value).
 * @studio expose category=tile name=set_ts_warm section=config
 * @param  tile  Initialised tile handle
 * @param  code  Raw 8-bit TS_WARM register value
 */
void tile_power_l_1t_set_ts_warm(tile_t* tile, uint8_t code);

/**
 * @brief  Set the hot-temperature TS threshold (raw register value).
 * @studio expose category=tile name=set_ts_hot section=config
 * @param  tile  Initialised tile handle
 * @param  code  Raw 8-bit TS_HOT register value
 */
void tile_power_l_1t_set_ts_hot(tile_t* tile, uint8_t code);

/**
 * @brief  Enable or disable TS-based thermal protection.
 *
 * When disabled, the chip ignores the NTC entirely (charging
 * proceeds regardless of battery temperature). Default at init: enabled.
 *
 * @studio expose category=tile name=set_ts_enabled section=config
 * @param  tile     Initialised tile handle
 * @param  enabled  1 = TS thermal protection on, 0 = off
 */
void tile_power_l_1t_set_ts_enabled(tile_t* tile, uint8_t enabled);

/* ---- LDO output --------------------------------------------------- */

/** LDO output mode — regulated voltage vs pass-through load switch. */
typedef enum {
    POWER_L_1T_LDO_MODE_LDO         = 0,  /**< Regulated LDO output */
    POWER_L_1T_LDO_MODE_LOAD_SWITCH = 1,  /**< Pass-through load switch */
} power_l_1t_ldo_mode_t;

/**
 * @brief  Set the LDO output voltage.
 *
 * VLDO = 600 + code × 100 mV. Values clamp to 600–3700 mV. Tile
 * design gates this rail to a fixed 1.8 V at the connector — changing
 * the LDO voltage breaks downstream peripherals expecting 1.8 V on
 * pad 10. Useful for non-default tile variants or load-switch-mode
 * pass-through use.
 *
 * @studio expose category=tile name=set_ldo_voltage_mv section=config
 * @param  tile  Initialised tile handle
 * @param  mv    Target LDO voltage in mV (600–3700)
 */
void tile_power_l_1t_set_ldo_voltage_mv(tile_t* tile, uint16_t mv);

/**
 * @brief  Set the LS/LDO output mode (regulated LDO vs load switch).
 *
 * In LDO mode the chip regulates pad 10 to the configured voltage
 * (10 mA max). In load-switch mode it pass-through-connects PMID
 * via a FET (up to 150 mA, but VINLS must be tied to the desired
 * supply on the tile PCB).
 *
 * @studio expose category=tile name=set_ldo_mode section=config
 * @param  tile  Initialised tile handle
 * @param  mode  LDO mode (POWER_L_1T_LDO_MODE_LDO or _LOAD_SWITCH)
 */
void tile_power_l_1t_set_ldo_mode(tile_t* tile, power_l_1t_ldo_mode_t mode);

/**
 * @brief  Enable or disable the LS/LDO output.
 *
 * When disabled, pad 10 (V+) goes high-impedance — downstream rails
 * expecting 1.8 V will drop. Default at init: enabled (chip default).
 *
 * @studio expose category=tile name=set_ldo_enabled section=config
 * @param  tile     Initialised tile handle
 * @param  enabled  1 = output on, 0 = output off (high-Z)
 */
void tile_power_l_1t_set_ldo_enabled(tile_t* tile, uint8_t enabled);

/* ---- Status / fault read ----------------------------------------- */

/**
 * @brief  Snapshot the chip's current state + latched faults.
 *
 * Reads STAT0/STAT1 (live state) and FLAG0/FLAG1/FLAG3 (event flags
 * which clear on read) into the supplied struct. Call once per
 * polling cycle — multiple reads will lose flag transitions.
 *
 * @studio expose category=tile name=get_charge_status section=runtime
 * @param  tile  Initialised tile handle
 * @param  out   Caller-allocated status struct (zeroed on entry)
 */
void tile_power_l_1t_get_charge_status(tile_t* tile,
                                       power_l_1t_status_t *out);

/* ---- Power management -------------------------------------------- */

/**
 * @brief  Enter ship mode (~10 nA quiescent).
 *
 * Disconnects the battery internally; only an MR press or VIN
 * insertion will wake the chip. Charge parameters reset to defaults
 * on exit. Use only for long-term storage / end-of-line packaging.
 *
 * @warning Destructive: requires physical user action (MR press or VIN
 * re-insertion) to recover. Marked `section=advanced` for the same
 * posture as other one-way / hardware-gated operations.
 *
 * @studio expose category=tile name=enter_ship_mode section=advanced
 * @param  tile  Initialised tile handle
 */
void tile_power_l_1t_enter_ship_mode(tile_t* tile);

/* ---- Raw register access (escape hatches) ------------------------ */

/**
 * @brief  Read any 8-bit BQ25150 register.
 *
 * @studio expose category=tile name=read_status returns=int section=advanced
 * @param  tile  Initialised tile handle
 * @param  reg   Register address
 * @return 8-bit register value
 */
uint8_t tile_power_l_1t_read_status(tile_t* tile, uint8_t reg);

/**
 * @brief  Write any 8-bit BQ25150 register.
 *
 * Escape hatch for advanced users wanting to touch registers the
 * driver doesn't expose. Caller is responsible for not bricking the
 * chip — most useful registers have typed setters above.
 *
 * @studio expose category=tile name=write_reg section=advanced
 * @param  tile   Initialised tile handle
 * @param  reg    Register address
 * @param  value  Value to write
 */
void tile_power_l_1t_write_reg(tile_t* tile, uint8_t reg, uint8_t value);

/* ============================================================== */
/* Runtime — tier-2 idiomatic helpers                              */
/*                                                                  */
/* These compose the tier-1 surface above into "is the battery     */
/* doing the thing I care about?" calls. All the boolean checks    */
/* are quick reads (one or two register accesses); only            */
/* wait_for_charge_done blocks, and even that polls at a slow      */
/* 1 s cadence because charge state changes on the order of        */
/* minutes — there's no value in spinning faster.                  */
/* ============================================================== */

/**
 * @brief  Is the cell currently being charged?
 *
 * @studio expose category=tile name=is_charging returns=bool section=runtime
 *
 * Convenience over @ref tile_power_l_1t_get_charge_status — returns
 * the `charging` field as a bool. Note this reads (and clears) FLAG3
 * as a side effect; if you also poll the full status struct, alternate
 * with this rather than calling both per cycle.
 *
 * @param  tile  Initialised tile handle
 * @return 1 if charging, 0 otherwise
 */
uint8_t tile_power_l_1t_is_charging(tile_t* tile);

/**
 * @brief  Has charge termination been reached?
 *
 * @studio expose category=tile name=is_charge_done returns=bool section=runtime
 *
 * Returns the `charge_done` bit from STAT0 — set when the cell
 * reaches VBATREG and current tapers below the termination threshold.
 *
 * @param  tile  Initialised tile handle
 * @return 1 if charge cycle has finished, 0 otherwise
 */
uint8_t tile_power_l_1t_is_charge_done(tile_t* tile);

/**
 * @brief  Is the battery below `threshold_pct`?
 *
 * @studio expose category=tile name=is_battery_low returns=bool section=runtime
 *
 * The classic "go to sleep" trigger. Equivalent to
 * `get_percent() < threshold_pct`. Quick: one ADC read.
 *
 * @param  tile           Initialised tile handle
 * @param  threshold_pct  Threshold in percent (0–100)
 * @return 1 if battery percent is strictly below threshold, 0 otherwise
 */
uint8_t tile_power_l_1t_is_battery_low(tile_t* tile, uint8_t threshold_pct);

/**
 * @brief  Is external power present (running off VIN)?
 *
 * @studio expose category=tile name=is_powered returns=bool section=runtime
 *
 * Returns the `vin_pgood` bit — true when VIN is in the valid
 * operating range (3.4–5.5 V). Use to branch behaviour between
 * "plugged in" and "battery only" modes.
 *
 * @param  tile  Initialised tile handle
 * @return 1 if VIN is good, 0 if running off battery only
 */
uint8_t tile_power_l_1t_is_powered(tile_t* tile);

/**
 * @brief  Block until charge_done is observed, or timeout.
 *
 * @studio expose category=tile name=wait_for_charge_done returns=bool section=runtime
 *
 * Polls the charge_done bit at a 1 s cadence (charge state changes
 * on the order of minutes for a typical cell — faster polling wastes
 * MCU cycles and bus bandwidth without finer-grained answers).
 * Returns 1 immediately if the cycle is already finished.
 *
 * @param  tile        Initialised tile handle
 * @param  timeout_ms  Maximum time to wait, in milliseconds
 * @return 1 if charge_done was observed, 0 on timeout
 */
uint8_t tile_power_l_1t_wait_for_charge_done(tile_t* tile, uint32_t timeout_ms);

#endif /* INC_TILE_POWER_L_1T_H_ */
