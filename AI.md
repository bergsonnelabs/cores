# Cores SDK — AI Assistant Onboarding Guide

This file gives any AI coding assistant (Claude, Copilot, Cursor, etc.) the context needed to work effectively in this repo without re-discovering the architecture from scratch.

**Keep this file up to date.** Refresh it before pushing any change that affects repo structure, project.json format, HAL APIs, coregen behaviour, or tile driver conventions.

---

## What This Repo Is

The **Cores SDK** is a firmware development kit for the Tiletown **Core** family of STM32-based tiles. It provides:

- A **HAL** (hardware abstraction layer) over STM32 LL drivers — no CubeIDE, no STM32Cube HAL
- A **code generator** (`coregen`) that turns a declarative `project.json` into initialisation C code
- The **Kiln tile driver framework** — platform-agnostic drivers for Tiletown sensor/actuator/power tiles
- A clean **build system** (Make + arm-none-eabi-gcc) targeting four Core MCU families

---

## Supported Cores (MCUs)

| Core variant | MCU | Tile JSON key | Architecture |
|---|---|---|---|
| Core.L.1 | STM32L011E4 | `Core-L-1-a` | Cortex-M0+, 32 MHz max, ultra-low-power |
| Core.U.1 | STM32L422TB | `Core-U-1-a` | Cortex-M4F, 80 MHz |
| Core.U.2 | STM32L422TB | `Core-U-2-a` | Cortex-M4F, 80 MHz (more pads) |
| Core.W | STM32WBA55HGF6 | `Core-W-b` | Cortex-M33, 100 MHz, BLE |
| Core.H.1 | STM32H523HE | `Core-H-1-a` | Cortex-M33, 250 MHz |

MCU capabilities (PLL ranges, APB clocks, SPI/I2C peripheral mapping) live in `MCU_DB` inside `tools/coregen/coregen.py`.

---

## Repo Layout

```
cores/
├── Makefile                    # Top-level build orchestrator
├── CLAUDE.md                   # ← this file
├── sdk/
│   ├── hal/                    # HAL headers + implementations
│   │   ├── hal_i2c.h/c
│   │   ├── hal_spi.h/c
│   │   ├── hal_gpio.h/c
│   │   ├── hal_timer.h/c
│   │   ├── hal_uart.h/c
│   │   ├── hal_usb_cdc.h/c
│   │   ├── hal_adc.h/c
│   │   ├── hal_debug.h/c
│   │   └── hal_common.h        # Status codes, shared types
│   ├── ll/                     # Low-level register access (wraps CMSIS)
│   ├── cmsis/                  # ARM CMSIS headers
│   ├── device/                 # Linker scripts, startup code (per MCU)
│   ├── tal/                    # Top-level abstraction layer
│   └── status/                 # SDK implementation status per Core subfamily
│       ├── features.json       # Canonical feature manifest (groups, IDs, layer, desc)
│       ├── core-l.json         # Core.L (STM32L011) feature statuses
│       ├── core-u.json         # Core.U (STM32L422) feature statuses
│       ├── core-w.json         # Core.W (STM32WBA55) feature statuses
│       └── core-h.json         # Core.H (STM32H523) feature statuses
├── kiln/                       # Tile driver framework (git submodule)
│   ├── tiles.h                 # Framework entry point — include this
│   ├── tiles_hal.h             # Platform abstraction interface
│   ├── hal/
│   │   └── tiles_hal_core.h/c  # Cores SDK implementation of tiles_hal_t
│   ├── definitions/            # Tile JSON definitions (28 tiles, canonical source)
│   └── drivers/                # Tile peripheral drivers (tile_*.h/c)
├── tools/
│   └── coregen/
│       ├── coregen.py          # Main generator — entry point
│       ├── templates/          # Jinja2 templates (see below)
│       └── project-config-schema.json
├── projects/                   # User projects (each has project.json)
│   └── my-project/
└── examples/                   # Reference projects (no project.json changes needed)
    ├── blink/
    ├── sdk-demo/               # SPI + I2C + tiles example
    └── ...
```

---

## Build System

### Variables

| Variable | Default | Description |
|---|---|---|
| `TILE` | `Core-U-2-a` | Core variant to build for |
| `PROJECT` | `blink` | Project name (looks in `examples/` or `projects/`) |
| `PROJECT_DIR` | `examples/$(PROJECT)` | Override if project lives elsewhere |
| `KILN_ENABLED` | `0` or `1` | Auto-set: 1 if project has tiles configured |
| `V` | `0` | Verbosity (1 = show all commands) |

### Common Commands

```bash
make                                           # Build blink for Core-U-2-a
make TILE=Core-W-b PROJECT=my-project         # Build specific core + project
make TILE=Core-W-b PROJECT=my-project V=1     # Verbose build
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

---

## project.json Format

Every project in `projects/<name>/` has a `project.json`. It is the single source of truth for hardware configuration. coregen derives all generated code from it.

```jsonc
{
  "project": {
    "name": "my-project",
    "core": "Core-W-b",          // Must match a kiln/definitions/<name>.json
    "description": "..."
  },
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
      "tile": "Sense.I.9",       // Must match a kiln/definitions tile name
      "bus": "I2C1",             // Must be an enabled interface
      "instance": 0,             // I2C: selects address slot; SPI: per-CS-line instance
      "cs_pad": "8"              // SPI tiles only: which pad is this tile's CS
    }
  ],
  "usb": {
    "enabled": false
  },
  "ble": {
    "enabled": true              // Core-W-b only; forces HSE clock (radio requires HSE)
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
- `pads` values must match pad function names defined in the tile's JSON (`kiln/definitions/`)
- `SPI*.CS` pads are configured as GPIO outputs (software CS via `hal_spi_set_cs()`)
- All pads used by an interface must appear in `pads` before that interface is usable
- A tile's `bus` must match a key in `interfaces`
- SPI tiles require a `cs_pad`; I2C tiles do not
- `ble.enabled` on Core-W-b auto-overrides any HSI16 clock level to the lowest HSE level
- `debug` and `isp` are accepted and validated but only emit comments/defines — no init code
- `isp.boot0_pad` emits `#define CORE_BOOT0_PAD <n>` in `core_board.h`
- `isp.method == "uart"` emits a `make flash-uart` hint comment in `core_board.h`

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

## coregen — Code Generator

**Entry point:** `tools/coregen/coregen.py`
**Called by:** `Makefile` during build (or `make generate`)

### What It Generates (`project/coregen/`)

| File | Always? | Contents |
|---|---|---|
| `core_pads.h` | ✓ | `PAD_n_PORT` / `PAD_n_PIN` macros for every pad |
| `core_board.h` | ✓ | Board-level defines (LED pad, power rails) |
| `core_interfaces.h` | ✓ | AF constants per signal (e.g. `I2C1_CLK_AF`) |
| `core_config.h` | project only | `SYSCLK_MHZ`, `PLL_M/N/R`, assigned pad functions |
| `core_init.h` | project only | Init function declarations, extern bus handles |
| `core_init.c` | project only | `core_clock_init()`, `core_pads_init()`, I2C/SPI/UART/tile init |
| `core.h` | project only | Master include (includes all of the above) |
| `tile_handles.h` | project+tiles | Tile handle variables; preserves edits outside markers |
| `core_drivers.mk` | project+tiles | Makefile fragment listing tile driver source files |

### Key Internal Functions

| Function | What it does |
|---|---|
| `build_pad_map()` | Extracts GPIO port/pin/AF from tile JSON for every pad |
| `build_pad_config()` | Resolves project pad assignments → GPIO mode, AF or output |
| `build_clock_config()` | Parses clock level, auto-solves PLL M/N/R; enforces HSE when BLE |
| `build_i2c_config()` | Calculates TIMINGR constant for target I2C speed |
| `build_spi_config()` | Detects SPI buses from pads, resolves CPOL/CPHA, CS GPIO |
| `build_uart_config()` | Detects UART/USART/LPUART buses from pads; reads baud + rx_interrupt |
| `build_i3c_config()` | Detects I3C buses; returns stub config for TODO comment in init |
| `build_debug_isp_config()` | Extracts debug/isp keys; normalises with defaults |
| `build_tiles_config()` | Maps tile instances to I2C or SPI bus handles; resolves driver paths |
| `validate_project_config()` | Checks all pads/interfaces/tiles/debug/isp are valid for the core |

### Editing coregen

- `MCU_DB` — add new MCUs here (define, family, PLL ranges, APB clock enables)
- `SPI_CLK_MAP` — maps (MCU define, SPI instance) → APB clock enable call
- `SPI_PRESCALER_MAP` — maps divisor → LL constant string
- Templates are Jinja2 in `tools/coregen/templates/`; context variables come from coregen.py's `ctx` dict
- UART clock enable is handled inside `hal_uart_init()` — no `UART_CLK_MAP` needed in coregen

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
int32_t  hal_adc_read_temp_decidegc(hal_adc_t *adc);           // die temp ×10 (e.g. 253 = 25.3°C)
uint32_t hal_adc_read_vdda_mv(hal_adc_t *adc);                 // actual VDD supply

// Read all configured channels in one call
void hal_adc_read_all(hal_adc_t *adc, uint16_t *buf);

// DMA circular buffer (HT + TC callback)
hal_status_t hal_adc_start_dma(hal_adc_t *adc, uint16_t *buf, uint16_t len,
                                void (*callback)(void));
void hal_adc_stop_dma(hal_adc_t *adc);
```

ADC resolution enum: `HAL_ADC_RES_6/8/10/12BIT` (all families); `HAL_ADC_RES_14BIT` (H5 only).

Sampling time presets: `HAL_ADC_SAMP_FAST / MED / SLOW / VERY_SLOW`.

Oversampling ratios: `HAL_ADC_OVERSAMPLE_1X / 4X / 16X / 64X / 256X` (all families); `1024X` (H5 only).

**VREFINT calibration approach** — `hal_adc_read_mv()` derives the actual VDDA voltage from the
VREFINT factory calibration stored in flash, then scales raw counts by that actual supply voltage.
This means millivolt results are correct even when VDD differs from 3.3 V (e.g. on battery).
VDDA is cached in `adc->vdda_mv` after the first call, with zero indicating "not yet measured".

**Per-family internal channel numbers** (used with `hal_adc_add_channel` or auto-added by
`hal_adc_read_temp_decidegc` / `hal_adc_read_vdda_mv`):

| Family     | VREFINT ch | TEMP ch | CAL voltage |
|------------|-----------|---------|-------------|
| L0 (L011)  | 17        | 16      | 3.0 V       |
| L4 (L422)  | 0         | 17      | 3.0 V       |
| WBA (WBA55)| 13        | 12      | 3.3 V       |
| H5 (H523)  | 19        | 16      | 3.3 V       |

**DMA pattern** — `hal_adc_start_dma` configures ADC in continuous+scan mode, wires DMA1 CH1
(L0/L4) or GPDMA1 CH0 (WBA/H5) in circular mode, and enables TC+HT interrupts. The callback
fires on both half-complete and transfer-complete, supporting a double-buffered processing pattern.

---

## Kiln Tile Driver Framework

### Framework Headers

**`tiles.h`** — include in all tile drivers and user code when `KILN_ENABLED=1`:
- Defines `tile_t` handle: `{ tiles_hal_t* hal, uint8_t id, tile_state_t state, ... }`
- States: `TILE_STATE_NONE → TILE_STATE_FOUND → TILE_STATE_READY` (or `TILE_STATE_ERROR`)
- `TILES_CHECK_VERSION(major, minor)` — compile-time SDK version assertion

**`tiles_hal.h`** — platform abstraction handle (`tiles_hal_t`):
- Function pointers for I2C, SPI, QSPI, delay, error callback
- Bus type flags: `TILES_BUS_I2C`, `TILES_BUS_SPI`, `TILES_BUS_QSPI`

**`kiln/hal/tiles_hal_core.h`** — Cores SDK implementation:
```c
typedef struct {
    hal_i2c_t *i2c;     // NULL if not I2C
    hal_spi_t *spi;     // NULL if not SPI
    uint32_t   buses;   // TILES_BUS_I2C | TILES_BUS_SPI
} tiles_hal_core_cfg_t;

void tiles_hal_core_init(tiles_hal_t *hal, tiles_hal_core_cfg_t *cfg);
```

### Tile Driver Conventions

Drivers live in `kiln/drivers/tile_<family>_<name>.h/c`:

```c
// Minimum viable tile driver
#include "tiles.h"

TILES_CHECK_VERSION(1, 0);

// Find: scan bus for the device, set tile state
uint8_t tile_sense_i_9_find(tiles_hal_t *hal, uint8_t instance);

// Init: configure registers, mark TILE_STATE_READY
void tile_sense_i_9_init(tiles_hal_t *hal, uint8_t instance, tile_t *tile);

// Data access functions (after init)
void tile_sense_i_9_get_raw_accels(tile_t *tile, int16_t accel[3]);
```

- `instance` selects the I2C address variant (0 → primary address, 1 → alternate, etc.)
- For SPI tiles, `instance` is per-CS-line (each CS = one device = one instance)
- Drivers access hardware only through `tiles_hal_t` function pointers — never directly

### Using Tile Drivers in User Code

Generated `tile_handles.h` declares handles; `core_init.c` initialises buses. User code adds tile init:

```c
#include "core.h"           // Generated: clocks, pads, bus handles
#include "tile_handles.h"   // Generated: extern tile_t declarations

// coregen:begin — do not edit this block
extern tiles_hal_t core_hal_i2c1;
// coregen:end

int main(void) {
    core_init();            // Clock + pads + I2C/SPI peripheral init

    // Your tile init goes here:
    tiles_hal_core_cfg_t cfg = { .i2c = &core_i2c1, .buses = TILES_BUS_I2C };
    tiles_hal_core_init(&core_hal_i2c1, &cfg);
    tile_sense_i_9_init(&core_hal_i2c1, 0, &imu);

    while (1) {
        int16_t accel[3];
        tile_sense_i_9_get_raw_accels(&imu, accel);
    }
}
```

---

## Design Rules (Important)

1. **No CubeIDE.** Never generate or use STM32CubeMX output. Write clean LL-based init from scratch.
2. **No STM32Cube HAL.** Use only `sdk/ll/` and `sdk/hal/`. The LL layer wraps CMSIS register access.
3. **Platform-agnostic tile drivers.** Drivers must compile on Arduino/ESP-IDF/Zephyr. No STM32 types in `tile_*.h`.
4. **project.json is the source of truth.** Never hand-edit generated files inside `coregen:begin/coregen:end` markers.
5. **CS lines are per-tile, not per-bus.** Each tile on an SPI bus gets its own GPIO CS line.
6. **`kiln/definitions/` is canonical.** Tile JSON lives there (synced from GitHub). Do not duplicate.
7. **`sdk/status/` is the source of truth for SDK implementation status.** The Tiletown web app and project builder read from copies of these files. Update `features.json` when adding a feature row, and the relevant `core-*.json` when implementation status changes. The Tiletown web app copies are synced manually via `tiletown/tools/sync_tile_json.py`.
8. **Update this file** before pushing any change that affects build, project.json schema, HAL API, tile driver conventions, or status JSON format.

---

## Common Tasks

### Add a new tile driver

1. Create `kiln/drivers/tile_<family>_<name>.h/c`
2. Add `TILES_CHECK_VERSION(1, 0)` and the `_find()` / `_init()` / data functions
3. Add the tile to `kiln/definitions/<TileName>.json` with bus capabilities
4. coregen will pick it up automatically via `build_tiles_config()`

### Add a new HAL peripheral

1. Create `sdk/hal/hal_<name>.h` and `sdk/hal/hal_<name>.c`
2. Follow the opaque-handle + config-struct pattern from `hal_i2c.h`
3. Add to `SOURCES` in the Makefile if always needed, or conditionally

### Add a new Core (MCU variant)

1. Add entry to `MCU_DB` in `tools/coregen/coregen.py`
2. Add linker script + startup to `sdk/device/`
3. Add `SPI_CLK_MAP` entries for the new MCU's APB assignments
4. Add the tile JSON to `kiln/definitions/`
5. Update `TILE` → MCU mapping in `Makefile`
6. Add a `sdk/status/core-<x>.json` with the new Core's feature statuses (see below)

### Update SDK implementation status

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

After editing, sync to the Tiletown web app by running `tiletown/tools/sync_tile_json.py` (that script handles tile JSONs; status files are copied manually to `apps/public/app/docs/sdk/cores/data/`).

### Debug a coregen issue

```bash
make generate TILE=Core-W-b PROJECT=my-project V=1
# Check projects/my-project/coregen/ for generated files
# coregen prints validation errors to stderr
```
