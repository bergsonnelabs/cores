# Cores SDK — AI Assistant Reference

Architecture, APIs, and conventions for the Cores SDK. This file gives any AI coding assistant the context needed to write firmware for Tiletown Core boards.

For operational workflows (driver authoring checklist, commit procedures, CI/CD) see the project's CLAUDE.md.

---

## What This Repo Is

The **Cores SDK** is a firmware development kit for the Tiletown **Core** family of STM32-based tiles. It provides:

- A **HAL** (hardware abstraction layer) over STM32 LL drivers — no CubeIDE, no STM32Cube HAL
- A **code generator** (`coregen`) that turns a declarative `config.json` into initialisation C code
- The **tile driver framework** — platform-agnostic drivers for Tiletown sensor/actuator/power tiles
- A clean **build system** (Make + arm-none-eabi-gcc) targeting four Core MCU families

---

## Supported Cores (MCUs)

| Public name | MCU | Definition stem | Architecture |
|---|---|---|---|
| `Core.ST.L0.1` | STM32L011E4 | `Core-L-1-a` | Cortex-M0+, 32 MHz max, ultra-low-power |
| `Core.ST.L4.1` | STM32L422TB | `Core-U-1-a` | Cortex-M4F, 80 MHz (first shipping L4) |
| `Core.ST.L4.2` | STM32L422TB | `Core-U-2-a` | Cortex-M4F, 80 MHz (more pads) |
| `Core.ST.W5.1` | STM32WBA55HGF6 | `Core-W-b` | Cortex-M33, 100 MHz, BLE |
| `Core.ST.H5.1` | STM32H523HE | `Core-H-1-a` | Cortex-M33, 250 MHz |

Pass either the **public name** or the **definition stem** as `TILE`. The
vendor-segmented public names resolve to their definition stem in the Makefile
(e.g. `Core.ST.L4.1` → `Core-U-1-a`); the matching alias map lives in
`tools/coregen/coregen.py`.

MCU capabilities (PLL ranges, APB clocks, SPI/I2C peripheral mapping) live in `MCU_DB` inside `tools/coregen/coregen.py`.

---

## Repo Layout

```
cores/
├── Makefile                    # Top-level build orchestrator
├── AI.md                       # ← this file
├── sdk/
│   ├── core/                   # User-facing API (core_ namespace, see Core Layer section)
│   ├── hal/                    # HAL implementations (see HAL Layer section)
│   ├── ll/                     # Low-level register access (wraps CMSIS)
│   ├── tal/                    # Tile abstraction layer (pad→peripheral mapping)
│   │   ├── tal_adc.h           # Pad → ADC channel resolution
│   │   ├── tal_timer.h         # Pad → timer instance/channel resolution
│   │   └── tal_exti.h          # Pad → EXTI with auto-pull
│   ├── cmsis/                  # ARM CMSIS headers
│   ├── device/                 # Linker scripts, startup code (per MCU)
│   └── status/                 # SDK implementation status per Core subfamily
│       ├── features.json       # Canonical feature manifest (groups, IDs, layer, desc)
│       ├── core-l.json         # Core.L (STM32L011) feature statuses
│       ├── core-u.json         # Core.U (STM32L422) feature statuses
│       ├── core-w.json         # Core.W (STM32WBA55) feature statuses
│       └── core-h.json         # Core.H (STM32H523) feature statuses
├── tiles.h                     # Tile framework entry point — include this
├── tiles_pal.h                 # Platform abstraction interface
├── definitions/                # Tile JSON definitions (canonical source)
├── drivers/                    # Tile peripheral drivers (tile_*.h/c)
├── hal/                        # Tile PAL adapters (Arduino / ESP-IDF / STM32)
├── templates/                  # Driver scaffolding (tile_template.{h,c})
├── manifests/                  # Generated manifests (per-tile + SDK)
├── tools/
│   └── coregen/
│       ├── coregen.py          # Main generator — entry point
│       ├── templates/          # Jinja2 templates (see below)
│       └── config-schema.json
├── projects/                   # User projects (each has config.json)
│   └── my-project/
└── examples/                   # Reference projects (no config.json changes needed)
    ├── blink/
    ├── sdk-demo/               # SPI + I2C + tiles example
    └── ...
```

---

## Build System

### Variables

| Variable | Default | Description |
|---|---|---|
| `TILE` | `Core.ST.L4.2` | Core to build for — public name or definition stem |
| `PROJECT` | `blink` | Project name (looks in `examples/` or `projects/`) |
| `PROJECT_DIR` | `examples/$(PROJECT)` | Override if project lives elsewhere |
| `TILES_ENABLED` | `0` or `1` | Auto-set: 1 if project has tiles configured |
| `BOOTLOADER` | from config.json | `1` = custom DFU bootloader (app at 0x08002000) |
| `ROM_DFU` | from config.json | `1` = ROM DfuSe bootloader (app at 0x08000000) |
| `V` | `0` | Verbosity (1 = show all commands) |

### Common Commands

```bash
make                                           # Build blink for the default Core (Core.ST.L4.2 / Core-U-2-a)
make TILE=Core.ST.W5.1 PROJECT=my-project     # Build specific core + project
make TILE=Core.ST.W5.1 PROJECT=my-project V=1 # Verbose build
make generate                                  # Run coregen only (no compile)
make flash                                     # Flash via OpenOCD / ST-Link
make flash-dfu                                 # Flash via USB DFU
make clean                                     # Remove build artefacts
make distclean                                 # Remove build + coregen output
```

### Build Flow

1. **coregen** runs → produces `project/coregen/` with generated `.h/.c/.mk` files
2. **compiler** builds project `main.c` + SDK HAL sources + `core_init.c` + tile drivers
3. **linker** uses MCU-specific linker script from `sdk/device/`

### coregen — Code Generator

**Entry point:** `tools/coregen/coregen.py`
**Called by:** `Makefile` during build (or `make generate`)

#### What It Generates (`project/coregen/`)

| File | Always? | Contents |
|---|---|---|
| `core_pads.h` | yes | `PAD_n_PORT` / `PAD_n_PIN` macros for every pad |
| `core_board.h` | yes | Board-level defines (LED pad, power rails) |
| `core_interfaces.h` | yes | AF constants per signal (e.g. `I2C1_CLK_AF`) |
| `core_config.h` | project only | `SYSCLK_MHZ`, `PLL_M/N/R`, assigned pad functions |
| `core_init.h` | project only | Init function declarations, extern bus handles |
| `core_init.c` | project only | `core_clock_init()`, `core_pads_init()`, I2C/SPI/UART/tile init |
| `core.h` | project only | Master include (includes all of the above) |
| `tile_handles.h` | project+tiles | Tile handle variables; preserves edits outside markers |
| `core_drivers.mk` | project+tiles | Makefile fragment listing tile driver source files |

#### Key Internal Functions

The `build_*_config()` functions in `coregen.py` each parse one section of `config.json` and return a config dict consumed by Jinja2 templates: `build_pad_map/config` (GPIO port/pin/AF), `build_clock_config` (PLL solver, HSE enforcement), `build_i2c/spi/uart/i3c/timer_config` (peripheral setup), `build_tiles_config` (driver paths + bus handles), and `validate_project_config` (cross-section validation).

#### Editing coregen

- `MCU_DB` — add new MCUs (define, family, PLL ranges, APB clock enables)
- `SPI_CLK_MAP` / `SPI_PRESCALER_MAP` — SPI clock enable + prescaler mappings
- Templates are Jinja2 in `tools/coregen/templates/`; context comes from the `ctx` dict in `generate()`

---

## config.json Format

Every project in `projects/<name>/` has a `config.json`. It is the single source of truth for hardware configuration. coregen derives all generated code from it. The filename is literal — coregen looks for `config.json` in the project directory; the project's human name comes from the directory name itself.

```jsonc
{
  "core": "Core.ST.W5.1",        // Public name (or a definitions/<name>.json stem)
  "clock": "default",            // "low" | "default" | "max"  (see tile JSON for MHz values)
  "pads": {
    // Pad number (string) → function
    "9":  "I2C1.CLK",
    "11": "I2C1.DAT",
    "8":  "SPI1.CS",             // SPI CS pads → GPIO output (software CS)
    "10": "SPI1.CLK",
    "6":  "SPI1.MOSI",
    "7":  "SPI1.MISO",
    "4":  "GPIO.OUT",
    "5":  "GPIO.IN"
  },
  "interfaces": {
    "I2C1": {
      "speed": 400000,           // 100000 | 400000 | 1000000
      "pullups": true
    },
    "SPI1": {
      "mode": 0,                 // 0–3 (CPOL/CPHA)
      "prescaler": 8,            // 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256
      "cs_polarity": "active-low" // omit for active-low (default); "active-high" if needed
    }
  },
  "tiles": [
    {
      "tile": "Sense.I.9",       // Must match a definitions tile name
      "bus": "I2C1",             // Must be an enabled interface
      "instance": 0,             // I2C: selects address slot; SPI: per-CS-line instance
      "cs_pad": "8"              // SPI tiles only: which pad is this tile's CS
    }
  ],
  "bootloader": "custom",          // "custom" | "rom" | "none" (default: "none")
  "usb": {
    "enabled": false
  },
  "ble": {
    "enabled": true              // Core.ST.W5.1 only; forces HSE clock (radio requires HSE)
  },
  "debug": {
    "interface": "swd",          // "swd" (default) | "jtag"
    "dedicated": true            // true = debug pads not reused for GPIO
  },
  "isp": {
    "method": "usb-dfu",         // "usb-dfu" | "uart" | "i2c" | "spi"
    "boot0_pad": "21"            // pad number to hold high at reset to enter ISP
  }
}
```

**Rules:**
- `pads` values must match pad function names defined in the tile's JSON (`definitions/`)
- `SPI*.CS` pads are configured as GPIO outputs (software CS via `hal_spi_set_cs()`)
- All pads used by an interface must appear in `pads` before that interface is usable
- A tile's `bus` must match a key in `interfaces`
- SPI tiles require a `cs_pad`; I2C tiles do not
- `ble.enabled` on Core.ST.W5.1 auto-overrides any HSI16 clock level to the lowest HSE level
- `bootloader`: `"custom"` (8KB DFU at 0x08000000, app at 0x08002000, Core.U/H), `"rom"` (ST ROM DfuSe, app at 0x08000000, Core.U fully/Core.H needs power cycle), `"none"` (default, SWD only). `BOOTLOADER`/`ROM_DFU` Make vars auto-set from this key.
- `debug` and `isp` emit comments/defines only — no init code. `isp.boot0_pad` emits `#define CORE_BOOT0_PAD`.

**Timer / PWM / Capture:**
```jsonc
"pads": {
  "7": "TIM15.1",              // Timer channel function → coregen sets AF
  "10": "TIM2.1"               // Can be PWM output or capture input
},
"timers": {
  "TIM15": { "freq": 1000, "tick": true },   // 1 kHz, also fire periodic ISR
  "TIM2":  { "freq": 1000000 }               // 1 MHz tick for capture
},
"pwm": {
  "channels": {
    "7": { "function": "TIM15.1", "freq": 1000, "duty": 50 }
  }
},
"capture": {
  "10": { "function": "TIM2.1" }
},
"iwdg": { "enabled": true, "timeout": "2s" }
```
- Timer functions in `pads` follow `TIMx.y` format (x = peripheral, y = channel 1-4)
- `timers` section sets per-instance frequency and optional tick
- `pwm.channels` sets per-pad duty (0-100%)
- `capture` maps pads to capture channels
- `iwdg.timeout`: `"1s"` | `"2s"` | `"5s"` | `"10s"`

**UART interfaces** (`USART1`, `USART2`, `LPUART1`, etc.):
```jsonc
"interfaces": {
  "USART2": {
    "baud": 115200,            // baud rate (default 115200)
    "rx_interrupt": false      // true = interrupt-driven RX with ring buffer
  }
}
```
Coregen generates `hal_uart_init(&core_usart2, USART2, SYSCLK_HZ, &cfg)` — no separate
clock enable call needed (handled inside `hal_uart_init`).

**I3C interfaces** (`I3C1`, `I3C2`):
```jsonc
"interfaces": {
  "I3C1": {
    "speed": 1000000           // speed in Hz — accepted but init not yet implemented
  }
}
```
Coregen emits a `/* I3C1: TODO */` comment in `core_init.c`. No build error.
See `sdk/hal/hal_i3c.h` for the stub API.

---

## Core Layer (`sdk/core/`) — User-Facing API

The `core_` layer provides platform-agnostic naming for application code. It is a thin alias over the HAL: each `core_` function delegates to the corresponding `hal_` function, and `core_*_t` handle types are typedefs of `hal_*_t`. This lets user code remain stable even if the underlying HAL implementation changes, and keeps example code readable without exposing STM32-specific details.

**Key headers:**

| Header | Purpose |
|---|---|
| `core_timer.h` | `core_timer_init_freq()` (PWM/tick), `core_timer_init_tick()` (capture), `core_timer_pwm_set()` (0-100%), `core_timer_capture_init/read()`, `core_timer_enable_tick()` |
| `core_gpio.h` | Single include for `core_pad_output/input/read/write/toggle()` + `core_pad_on_change()` (EXTI) |
| `core_adc.h` | `core_adc_init()`, `core_adc_read()`, `core_adc_read_mv()`, DMA, temp sensor |
| `core_i2c.h` | `core_i2c_setup()` with auto-timing, `core_i2c_write/read/probe/scan()` |
| `core_watchdog.h` | `core_watchdog_start(ms)`, `core_watchdog_feed()`, `core_watchdog_caused_reset()` |
| `core_usb.h` | `core_usb_init()`, `core_usb_write()`, `core_usb_printf`, includes `<stdio.h>` |
| `core_timing.h` | `core_delay_ms/us()`, `core_millis()`, `core_timeout()` |

**Timer init has two variants:**
- `core_timer_init_freq(&t, TIM2, 1000)` — overflow at 1 kHz (for PWM, periodic tick)
- `core_timer_init_tick(&t, TIM2, 1000000)` — tick at 1 MHz, free-running (for input capture)

**PWM duty is 0-100 integer percent** at the core_ layer. For 0.1% resolution, use `hal_timer_pwm_set_duty()` with 0-1000.

**`core.h`** (generated by coregen) includes `core_gpio.h` and `core_timing.h` automatically, so delays and GPIO are always available.

---

## HAL Layer (`sdk/hal/`)

Design principles:
- Wraps STM32 LL (low-level) drivers — **never** STM32Cube HAL
- Opaque handle structs (`hal_i2c_t`, `hal_spi_t`, etc.) initialised with a config struct
- Single-threaded polling API; some DMA paths in `hal_spi`
- No STM32-specific types in public `.h` files

### Key HAL APIs

**I2C** (`hal_i2c.h`):
```c
hal_status_t hal_i2c_init(hal_i2c_t *h, uint32_t instance, hal_i2c_config_t *cfg);
hal_status_t hal_i2c_write_reg(hal_i2c_t *h, uint8_t addr, uint16_t reg, uint8_t *data, uint16_t len);
hal_status_t hal_i2c_read_reg(hal_i2c_t *h, uint8_t addr, uint16_t reg, uint8_t *data, uint16_t len);
hal_status_t hal_i2c_is_ready(hal_i2c_t *h, uint8_t addr);
```

**SPI** (`hal_spi.h`):
```c
hal_status_t hal_spi_init(hal_spi_t *h, uint32_t instance, hal_spi_config_t *cfg);
hal_status_t hal_spi_set_cs(hal_spi_t *h, GPIO_TypeDef *port, uint32_t pin);
hal_status_t hal_spi_write(hal_spi_t *h, uint8_t *data, uint16_t len);
hal_status_t hal_spi_read(hal_spi_t *h, uint8_t *data, uint16_t len);
// h->cs_active_low = 0  →  active-high CS
```

**UART** (`hal_uart.h`):
```c
// NOTE: instance is USART_TypeDef* (e.g. USART2), not a bus number.
// pclk_hz = SYSCLK_HZ (all APB dividers are 1 in coregen output).
// hal_uart_init() enables its own peripheral clock internally.
hal_status_t hal_uart_init(hal_uart_t *h, USART_TypeDef *instance,
                           uint32_t pclk_hz, const hal_uart_config_t *cfg);
void         hal_uart_tx(hal_uart_t *h, const uint8_t *data, uint32_t len);
void         hal_uart_tx_str(hal_uart_t *h, const char *str);
int          hal_uart_printf(hal_uart_t *h, const char *fmt, ...);
int          hal_uart_rx_ready(hal_uart_t *h);
uint8_t      hal_uart_rx(hal_uart_t *h);         // blocking
int          hal_uart_rx_try(hal_uart_t *h, uint8_t *byte);  // non-blocking
```

**I3C** (`hal_i3c.h` — stub, not yet implemented):
```c
hal_status_t hal_i3c_init(hal_i3c_t *h, uint32_t instance_id,
                           const hal_i3c_config_t *cfg);  // returns HAL_ERROR
hal_status_t hal_i3c_write(hal_i3c_t *h, uint8_t addr,
                            const uint8_t *data, uint16_t len);
hal_status_t hal_i3c_read(hal_i3c_t *h, uint8_t addr,
                           uint8_t *data, uint16_t len);
```

**ADC** (`hal_adc.h` — Tier 1, all four Core families):
```c
// Initialise: enables clock, calibrates, enables ADC
hal_status_t hal_adc_init(hal_adc_t *adc, ADC_TypeDef *instance,
                          uint32_t sysclk_hz, hal_adc_res_t res);

// Add channels before reading (per-channel sampling time)
hal_status_t hal_adc_add_channel(hal_adc_t *adc, uint8_t channel, hal_adc_samp_t samp);

// Oversampling (applies to all channels; can be called after init)
hal_status_t hal_adc_set_oversample(hal_adc_t *adc, hal_adc_oversample_t ratio);

// Single-shot blocking reads
uint16_t hal_adc_read(hal_adc_t *adc, uint8_t channel);        // raw count
uint32_t hal_adc_read_mv(hal_adc_t *adc, uint8_t channel);     // millivolts (VREFINT-calibrated)

// Internal measurements
int32_t  hal_adc_read_temp_decidegc(hal_adc_t *adc);           // die temp x10 (e.g. 253 = 25.3C)
uint32_t hal_adc_read_vdda_mv(hal_adc_t *adc);                 // actual VDD supply

// Read all configured channels in one call
void hal_adc_read_all(hal_adc_t *adc, uint16_t *buf);

// DMA circular buffer (HT + TC callback)
hal_status_t hal_adc_start_dma(hal_adc_t *adc, uint16_t *buf, uint16_t len,
                                void (*callback)(void));
void hal_adc_stop_dma(hal_adc_t *adc);
```

Enums: `HAL_ADC_RES_6/8/10/12BIT` (all families, plus `14BIT` on H5). Sampling: `HAL_ADC_SAMP_FAST/MED/SLOW/VERY_SLOW`. Oversampling: `HAL_ADC_OVERSAMPLE_1X/4X/16X/64X/256X` (plus `1024X` on H5).

`hal_adc_read_mv()` uses VREFINT factory calibration to derive true VDDA, so millivolt results are correct even when VDD differs from 3.3 V. VDDA is cached in `adc->vdda_mv` after first measurement.

**Per-family internal channel numbers:**

| Family     | VREFINT ch | TEMP ch | CAL voltage |
|------------|-----------|---------|-------------|
| L0 (L011)  | 17        | 16      | 3.0 V       |
| L4 (L422)  | 0         | 17      | 3.0 V       |
| WBA (WBA55)| 13        | 12      | 3.3 V       |
| H5 (H523)  | 19        | 16      | 3.3 V       |

`hal_adc_start_dma` uses DMA1 CH1 (L0/L4) or GPDMA1 CH0 (WBA/H5) in circular mode with HT+TC interrupts for double-buffered processing.

---

## Tile Driver Framework

### Framework Headers

**`tiles.h`** — include in all tile drivers and user code when `TILES_ENABLED=1`:
- Defines `tile_t` handle: `{ tiles_pal_t* hal, uint8_t id, tile_state_t state, ... }`
- States: `TILE_STATE_NONE -> TILE_STATE_FOUND -> TILE_STATE_READY` (or `TILE_STATE_ERROR`)
- `TILES_CHECK_VERSION(major, minor)` — compile-time SDK version assertion

**`tiles_pal.h`** — platform abstraction handle (`tiles_pal_t`):
- Function pointers for I2C, SPI, QSPI, delay, error callback
- Bus type flags: `TILES_BUS_I2C`, `TILES_BUS_SPI`, `TILES_BUS_QSPI`

**`sdk/core/core_tiles.h`** — Cores SDK bridge (wires bus handles to `tiles_pal_t`):
```c
// Single function for both I2C and SPI (C11 _Generic dispatch):
tiles_pal_t *hal = core_tiles_pal(&core_i2c1);   // I2C bus
tiles_pal_t *hal = core_tiles_pal(&core_spi1);   // SPI bus
```

### Tile Driver Conventions

Drivers live in `drivers/tile_<family>_<name>.h/c`:

```c
// Minimum viable tile driver
#include "tiles.h"

TILES_CHECK_VERSION(1, 0);

// Find: scan bus for the device, set tile state
uint8_t tile_sense_i_9_find(tiles_pal_t *hal, uint8_t instance);

// Init: configure registers, mark TILE_STATE_READY
void tile_sense_i_9_init(tiles_pal_t *hal, uint8_t instance, tile_t *tile);

// Data access functions (after init)
void tile_sense_i_9_get_raw_accels(tile_t *tile, int16_t accel[3]);
```

- `instance` selects the I2C address variant (0 = primary address, 1 = alternate, etc.)
- For SPI tiles, `instance` is per-CS-line (each CS = one device = one instance)
- Drivers access hardware only through `tiles_pal_t` function pointers — never directly

### Using Tile Drivers in User Code

Generated `core_init.c` initialises clocks, pads, and bus peripherals. User code includes `core_tiles.h` for the bridge and calls `core_tiles_pal()` to get a `tiles_pal_t*`:

```c
#include "core.h"           // Generated: clocks, pads, bus handles
#include "core_tiles.h"     // core_tiles_pal() — I2C and SPI
#include "tile_sense_i_9.h"

int main(void) {
    core_init();

    tile_t imu;
    tiles_pal_t *hal = core_tiles_pal(&core_i2c1);
    tile_sense_i_9_init(hal, 0, &imu);

    while (1) {
        int16_t accel[3];
        tile_sense_i_9_get_raw_accels(&imu, accel);
    }
}
```

---

## Design Rules

1. **No CubeIDE.** Never generate or use STM32CubeMX output. Write clean LL-based init from scratch.
2. **No STM32Cube HAL.** Use only `sdk/ll/` and `sdk/hal/`. The LL layer wraps CMSIS register access.
3. **Platform-agnostic tile drivers.** Drivers must compile on Arduino/ESP-IDF/Zephyr. No STM32 types in `tile_*.h`.
4. **config.json is the source of truth.** Never hand-edit generated files inside `coregen:begin/coregen:end` markers.
5. **CS lines are per-tile, not per-bus.** Each tile on an SPI bus gets its own GPIO CS line.
6. **`definitions/` is canonical.** Tile JSON lives there (synced from GitHub). Do not duplicate.
7. **`sdk/status/` is the source of truth for SDK implementation status.** Update `features.json` when adding a feature row, and the relevant `core-*.json` when implementation status changes.

---

## Common Tasks

### Add a new tile driver

#### Phase 1 — Research

- [ ] Read IC datasheets: register map, I2C/SPI protocol, power-on defaults, device ID register
- [ ] Check `definitions/` for the tile JSON (already exists from the DB?)
- [ ] Check `TILE_DRIVER_MAP` in `tools/coregen/coregen.py` (entry exists?)
- [ ] Choose a reference driver to follow:
  - **Sensor (I2C, simple):** `tile_sense_mic.h/c` — config struct, calibration, data reads
  - **Sensor (I2C, full):** `tile_sense_bp.h/c` — ODR/AVG/FS config, FIFO, one-shot, threshold interrupts, autozero/autorefp, offset calibration
  - **Sensor (I2C+SPI):** `tile_sense_i_6p6.h/c` — dual-bus dispatch, interrupt callbacks
  - **Actuator:** `tile_drive_p.h/c` — modes, FIFO, status monitoring
  - **Audio/multi-IC:** `tile_drive_a_2.h/c` — DAC+amp, safe startup sequence

#### Phase 2 — Driver implementation

Create `drivers/tile_<family>_<name>.h` and `.c`. All code must be platform-agnostic — no STM32 types, no direct register access. All bus I/O goes through `tile->hal` function pointers.

**Header (.h) structure:**

```
File-level Doxygen (brief, specs, quick-start @code example, datasheet links)
Include guard + #include "tiles.h"
Version macros: TILE_<FAMILY>_<NAME>_VERSION_{MAJOR,MINOR,PATCH}
TILES_CHECK_VERSION(1, 0)
Instance mapping table (Doxygen comment showing instance → address)
IC address defines (#define <IC>_I2C_ADDR_DEFAULT ...)
Register map defines (#define <IC>_REG_... grouped by function)
Enums (gain, mode, waveform, etc. — values map to hardware register fields)
Config structs (optional init config, AGC config, etc.)
Public API declarations with full Doxygen (@brief, @param, @return, @note)
```

**Implementation (.c) structure:**
```
id_table[] — maps instance index to I2C address (or CS index for SPI)
resolve_id(instance) — bounds-checked lookup, returns 0 for invalid
Per-instance state struct (static, private — cached config, addresses)
state_for(tile_t *) — lookup helper matching tile->id to state slot
Bus helpers — bus-aware read/write:
  For I2C+SPI: check tile->hal->buses & TILES_BUS_SPI to dispatch
  For amp/secondary ICs: use address from state, not tile->id
Lifecycle: find(), init(), sleep(), wake(), reset()
Data/control functions
```

**Init sequence pattern:**

1. Zero tile struct, set `tile->hal` and `tile->id` (resolve from instance)
2. Probe device (`i2c_is_ready` or SPI test read)
3. Read device ID register, verify expected value
4. Configure device (power up outputs, set defaults)
5. **For tiles with amplifiers/power stages:** settle outputs at safe level (e.g., mid-scale) before enabling the power stage. Put amp in shutdown -> configure -> wake. This prevents AGC gain runaway from DC offset transients.
6. Cache configuration in per-instance state
7. Set `tile->state = TILE_STATE_READY` (or `TILE_STATE_ERROR` + `TILE_ON_ERROR()`)

**Do NOT software-reset the device** unless necessary — resets can wipe NVM-stored analog configuration that the tile hardware depends on.

### Add a new HAL peripheral

1. Create `sdk/hal/hal_<name>.h` and `sdk/hal/hal_<name>.c`
2. Follow the opaque-handle + config-struct pattern from `hal_i2c.h`
3. Add to `SOURCES` in the Makefile if always needed, or conditionally

### Add a new Core (MCU variant)

1. Add entry to `MCU_DB` in `tools/coregen/coregen.py`
2. Add linker script + startup to `sdk/device/`
3. Add `SPI_CLK_MAP` entries for the new MCU's APB assignments
4. Add the tile JSON to `definitions/`
5. Update `TILE` -> MCU mapping in `Makefile`
6. Add a `sdk/status/core-<x>.json` with the new Core's feature statuses

### SDK implementation status

Status files live in `sdk/status/`. `features.json` is the canonical feature manifest; `core-*.json` files hold per-subfamily statuses.

**Status values:**
- `"verified"` — hardware-tested and confirmed working
- `"compile"` — builds cleanly, not yet hardware-validated
- `"partial"` — partially working; a `note` field explains the limitation
- `"wip"` — actively in development
- `"none"` — not yet implemented (omitting a key is equivalent)
- `"na"` — not applicable for this Core (hide from display)

**`features.json` entry format:**
```json
{ "id": "uart.polling", "layer": "LL", "name": "TX / RX (polling)" }
{ "id": "system.tick",  "layer": "LL", "name": "SysTick", "desc": "Tooltip shown in the web table." }
```
Layer values: `"LL"`, `"HAL"`, `"TAL"`, `"Core"` — implies all layers below are also available.

**`core-*.json` entry format:**
```json
{
  "id": "Core.W",
  "mcu": "STM32WBA55",
  "freq": "100 MHz",
  "arch": "M33",
  "bootloaders": ["uart", "i2c", "spi"],
  "features": {
    "uart.polling": "verified",
    "adc.mv": { "status": "partial", "note": "VREFINT bypassed on WBA55; hardcoded 3300 mV" },
    "conn.usb_cdc": "na"
  }
}
```

### Debug a coregen issue

```bash
make generate TILE=Core.ST.W5.1 PROJECT=my-project V=1
# Check projects/my-project/coregen/ for generated files
# coregen prints validation errors to stderr
```
