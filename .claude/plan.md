# USB CDC Implementation Plan

## Goal
Add USB CDC (virtual serial port) support to the Core firmware SDK, targeting Core.ST.L4.2 (STM32L422). When a Core.ST.L4.2 is plugged into a Mac via USB, it shows up as `/dev/tty.usbmodem*` and the firmware can send/receive data — replacing the need for a UART adapter.

## Architecture

Three new files following our existing LL/HAL pattern:

```
sdk/
├── ll/ll_usb.h          # USB Device FS register access (PMA, endpoints, interrupts)
├── hal/hal_usb_cdc.h    # CDC class API (init, printf, read/write)
└── hal/hal_usb_cdc.c    # USB core + CDC implementation (descriptors, enumeration, data)
```

Plus clock/CRS support:
```
sdk/ll/ll_crs.h          # Clock Recovery System (syncs HSI48 to USB SOF)
```

## Phase Breakdown

### Phase A: LL USB — Register Definitions & PMA Access

`ll_usb.h` — thin register layer for the STM32 USB Device FS peripheral (base 0x40006800, PMA base 0x40006C00):

- **USB register struct** — CNTR, ISTR, FNR, DADDR, BTABLE, EP0R–EP7R
- **PMA access helpers** — The PMA on STM32L4 is 16-bit addressed with 32-bit alignment (every other 16-bit word is padding). Need read/write helpers that handle this.
- **Endpoint register helpers** — EP registers have toggle bits (STAT_TX, STAT_RX) that flip on write-1, plus invariant bits. Need careful read-modify-write helpers.
- **Buffer descriptor table** — Set TX/RX buffer addresses and counts per endpoint.

### Phase B: LL CRS — Crystal-less USB Clock

`ll_crs.h` — Clock Recovery System that auto-trims HSI48 to USB SOF packets:

- Enable CRS peripheral clock
- Configure sync source = USB SOF
- Enable auto-trim

Also update `ll_rcc.h` to add:
- USB 48MHz clock source selection (RCC_CCIPR CLK48SEL → HSI48)

And update `tile_init.c.j2` template so that when a project uses USB, tilegen auto-generates HSI48 + CRS init.

### Phase C: HAL USB CDC — Enumeration & Data

`hal_usb_cdc.h` / `hal_usb_cdc.c` — the USB Device core and CDC class in one file:

**USB Core (internal):**
- USB reset handler (configure EP0, set address 0)
- Setup packet handler (GET_DESCRIPTOR, SET_ADDRESS, SET_CONFIGURATION)
- EP0 control transfer state machine (SETUP → DATA IN/OUT → STATUS)
- USB descriptors: Device, Configuration, Interface (CDC Control + Data), Endpoint, CDC functional descriptors
- VID/PID: Use a test VID/PID initially (configurable via config.json later)

**CDC Class:**
- CDC-specific requests: SET_LINE_CODING, GET_LINE_CODING, SET_CONTROL_LINE_STATE
- EP1 IN (bulk, 64 bytes) — device → host TX
- EP1 OUT (bulk, 64 bytes) — host → device RX
- EP2 IN (interrupt, 8 bytes) — CDC notification (mostly unused but required)
- Ring buffer for RX (reuse hal_ringbuf_t from hal_common.h)

**Public API:**
```c
// Initialize USB CDC (enables HSI48, CRS, USB peripheral, NVIC)
hal_status_t hal_usb_cdc_init(void);

// Check if host is connected and port is open (DTR set)
int hal_usb_cdc_connected(void);

// Blocking write
void hal_usb_cdc_tx(const uint8_t *data, uint32_t len);

// Printf (mirrors hal_uart_printf signature)
int hal_usb_cdc_printf(const char *fmt, ...);

// Non-blocking read from RX ring buffer
uint16_t hal_usb_cdc_read(uint8_t *buf, uint16_t max_len);
int hal_usb_cdc_rx_ready(void);
uint8_t hal_usb_cdc_getc(void);
```

**USB interrupt handler:**
- `USB_IRQHandler` in hal_usb_cdc.c (IRQ 67 on L422)
- Handles: RESET, CTR (correct transfer), SUSP, WKUP

### Phase D: Test Project — `usb-serial`

Simple project that demonstrates USB CDC:
```c
int main(void) {
    tile_init();
    hal_usb_cdc_init();

    while (!hal_usb_cdc_connected())
        ;  // Wait for host to open port

    hal_usb_cdc_printf("Hello from Core.ST.L4.2!\n");

    while (1) {
        // Echo + ADC reading
        if (hal_usb_cdc_rx_ready()) {
            uint8_t c = hal_usb_cdc_getc();
            hal_usb_cdc_printf("You typed: %c\n", c);
        }
        hal_usb_cdc_printf("Tick: %lu\n", hal_tick());
        ll_delay_ms(1000);
    }
}
```

config.json adds USB pin assignments and enables HSI48:
```json
{
  "clock": {
    "source": "hsi16",
    "sysclk_mhz": 80
  },
  "pins": {
    "16": "USB.DM",
    "17": "USB.DP"
  },
  "usb": true
}
```

### Phase E: `make dfu` Target

Add a proper `make flash-dfu` target using `dfu-util` for flashing via the STM32's built-in ROM bootloader (BOOT0 held high). This needs no firmware changes — just a Makefile target.

## PMA Memory Layout (1024 bytes total)

```
Offset  Size  Purpose
0x000   64    Buffer Descriptor Table (8 endpoints × 8 bytes)
0x040   64    EP0 TX buffer (control IN)
0x080   64    EP0 RX buffer (control OUT)
0x0C0   64    EP1 TX buffer (CDC bulk IN)
0x100   64    EP1 RX buffer (CDC bulk OUT)
0x140   8     EP2 TX buffer (CDC interrupt IN)
              ~700 bytes free for additional endpoints
```

## Key Technical Challenges

1. **EP register toggle bits** — STAT_TX/STAT_RX use a toggle-on-write mechanism. To set them to a specific value, you must XOR the desired state with the current state and write that. Other bits in the EP register are write-invariant or write-0-to-clear. Each EP register access needs a careful helper function.

2. **PMA 16-bit access** — On the STM32L4, PMA is accessed as 16-bit words at 32-bit aligned addresses. Every other 16-bit slot is unused. The copy-to/from-PMA helpers must account for this.

3. **Enumeration timing** — USB host expects responses within specific timeframes. The setup packet handler needs to be interrupt-driven, not polled.

4. **CDC ZLP** — When TX data is an exact multiple of 64 bytes, a zero-length packet must follow to signal end of transfer.

## Scope Decisions

- **CDC only** — no HID, MSC, or other classes for now
- **Single configuration** — no composite device yet (DFU later)
- **Interrupt-driven** — USB handler runs in the USB ISR, data buffered via ring buffers
- **No flow control** — simple TX/RX, no DTR/RTS handshaking beyond connection detection
- **L422 only** — USB peripheral doesn't exist on L011, and WBA/H5 use a different USB IP (USB OTG). Those would need separate implementations later.

## Build Integration

- `hal_usb_cdc.c` goes into `sdk/hal/` and gets compiled automatically (wildcard already picks up `sdk/hal/*.c`)
- USB IRQ number added to `hal_common.h` (`HAL_IRQ_USB = 67` for L422)
- `config.json` `"usb": true` flag tells tilegen to generate HSI48 + CRS init in `tile_init.c`
