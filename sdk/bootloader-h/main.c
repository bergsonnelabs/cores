/**
 * DFU Bootloader for Core.ST.H5 (STM32H523HE)
 *
 * Boot flow:
 *   1. Check noinit SRAM (0x20043FF0) for magic value (0xDEADBEEF)
 *   2. If magic present → clear it, enter USB DFU mode
 *   3. If no magic but valid app → jump to application at 0x08002000
 *   4. If no magic and no valid app → enter USB DFU mode (first-time)
 *
 * The application triggers DFU mode by calling hal_dfu_reboot(),
 * which writes the magic and resets. The BOOT0/ROM bootloader
 * remains as the emergency fallback.
 *
 * USB DFU 1.1 class device — uses control transfers only (EP0).
 * Flash programming: sector erase (8KB) + double-word (8-byte) writes.
 *
 * Ported from the Core.ST.L4 (STM32L422) bootloader. Uses the H5 USB DRD
 * peripheral (32-bit registers, 2KB PMA) via ll_usb_drd.h.
 */

#include "ll_common.h"
#include "ll_rcc.h"
#include "ll_crs.h"
#include "ll_gpio.h"
#include "ll_usb_drd.h"
#include "ll_systick.h"
#include "ll_flash.h"

/* ============================================================
 * Constants
 * ============================================================ */

#define DFU_MAGIC           0xDEADBEEFUL
#define DFU_MAGIC_ADDR      (*(volatile uint32_t *)0x20043FF0UL)

#define APP_ADDRESS         0x08002000UL
#define APP_END_ADDRESS     0x08080000UL    /* 512KB flash end */

/* SRAM range for stack pointer validation */
#define SRAM_START          0x20000000UL
#define SRAM_END            0x20044000UL    /* 272KB SRAM end */

/* SCB registers */
#define SCB_VTOR            REG32(0xE000ED08UL)
#define SCB_AIRCR           REG32(0xE000ED0CUL)

/* Bootloader sector (sector 0, 8KB — do not self-erase) */
#define BOOTLOADER_SECTORS  1

/* DFU transfer size (matches EP0 max packet) */
#define DFU_TRANSFER_SIZE   64

/* PWR registers for VDDUSB */
#ifndef PWR_BASE
  #define PWR_BASE          0x44020800UL
#endif

/* ============================================================
 * USB PMA layout — EP0 only
 *
 * BDT:    0x00–0x07  (1 endpoint x 8 bytes)
 * EP0 TX: 0x40–0x7F  (64 bytes)
 * EP0 RX: 0x80–0xBF  (64 bytes)
 * ============================================================ */

#define PMA_EP0_TX          0x40
#define PMA_EP0_RX          0x80
#define EP0_MAX_PACKET      64

/* ============================================================
 * USB descriptor types and request codes
 * ============================================================ */

#define USB_DESC_DEVICE         0x01
#define USB_DESC_CONFIGURATION  0x02
#define USB_DESC_STRING         0x03
#define USB_DESC_INTERFACE      0x04
#define USB_DESC_DEVICE_QUALIFIER 0x06

#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09

/* DFU class descriptor type */
#define USB_DESC_DFU_FUNCTIONAL 0x21

/* DFU class requests */
#define DFU_DNLOAD              0x01
#define DFU_UPLOAD              0x02
#define DFU_GETSTATUS           0x03
#define DFU_CLRSTATUS           0x04
#define DFU_GETSTATE            0x05
#define DFU_ABORT               0x06

/* DFU states */
#define DFU_STATE_IDLE          2
#define DFU_STATE_DNLOAD_SYNC   3
#define DFU_STATE_DNLOAD_IDLE   5
#define DFU_STATE_MANIFEST_SYNC 6
#define DFU_STATE_MANIFEST_DONE 7
#define DFU_STATE_ERROR         10

/* DFU status codes */
#define DFU_STATUS_OK           0x00
#define DFU_STATUS_ERR_TARGET   0x01
#define DFU_STATUS_ERR_VERIFY   0x04
#define DFU_STATUS_ERR_PROG     0x06
#define DFU_STATUS_ERR_ADDRESS  0x08

/* ============================================================
 * USB descriptors
 * ============================================================ */

/* Device descriptor — VID 0x1209 (pid.codes), PID 0x0002 */
static const uint8_t dev_desc[] = {
    18, USB_DESC_DEVICE,
    0x00, 0x02,             /* bcdUSB = 2.00 */
    0x00, 0x00, 0x00,       /* Class defined at interface level */
    EP0_MAX_PACKET,
    0x09, 0x12,             /* idVendor = 0x1209 */
    0x02, 0x00,             /* idProduct = 0x0002 */
    0x00, 0x01,             /* bcdDevice = 1.00 */
    1, 2, 3, 1,             /* iManufacturer, iProduct, iSerial, bNumConfigs */
};

/* Configuration + Interface + DFU Functional = 9 + 9 + 9 = 27 bytes */
#define CONFIG_DESC_TOTAL   27

static const uint8_t cfg_desc[] = {
    9, USB_DESC_CONFIGURATION, CONFIG_DESC_TOTAL, 0, 1, 1, 0, 0x80, 250,
    9, USB_DESC_INTERFACE, 0, 0, 0, 0xFE, 0x01, 0x02, 4,
    9, USB_DESC_DFU_FUNCTIONAL, 0x01, 0x00, 0x00, DFU_TRANSFER_SIZE, 0, 0x10, 0x01,
};

static const uint8_t str0_desc[] = { 4, USB_DESC_STRING, 0x09, 0x04 };
static const uint8_t str1_desc[] = {
    20, USB_DESC_STRING,
    'B',0, 'e',0, 'r',0, 'g',0, 's',0, 'o',0, 'n',0, 'n',0, 'e',0,
};
static const uint8_t str2_desc[] = {
    28, USB_DESC_STRING,
    'C',0, 'o',0, 'r',0, 'e',0, ' ',0, 'T',0, 'i',0, 'l',0, 'e',0,
    ' ',0, 'D',0, 'F',0, 'U',0,
};
static const uint8_t str3_desc[] = {
    14, USB_DESC_STRING,
    '0',0, '0',0, '0',0, '0',0, '0',0, '1',0,
};
static const uint8_t str4_desc[] = {
    28, USB_DESC_STRING,
    'D',0, 'F',0, 'U',0, ' ',0, 'I',0, 'n',0, 't',0, 'e',0, 'r',0,
    'f',0, 'a',0, 'c',0, 'e',0,
};

/* ============================================================
 * USB/DFU state
 * ============================================================ */

typedef enum {
    EP0_IDLE = 0,
    EP0_DATA_IN,
    EP0_DATA_OUT,
    EP0_STATUS_IN,
    EP0_STATUS_OUT,
} ep0_state_t;

static struct {
    volatile uint8_t configured;
    uint8_t          address_pending;
    ep0_state_t      ep0_state;
    const uint8_t   *ep0_tx_ptr;
    uint16_t         ep0_tx_remain;

    uint8_t          dfu_state;
    uint8_t          dfu_status;
    uint16_t         block_num;
    uint8_t          buf[DFU_TRANSFER_SIZE];
    uint16_t         buf_len;
    uint64_t         erased_pages;
} _dfu;

/* ============================================================
 * EP0 control transfer helpers (DRD LL calls)
 * ============================================================ */

static void ep0_tx_packet(void)
{
    uint16_t len = _dfu.ep0_tx_remain;
    if (len > EP0_MAX_PACKET) len = EP0_MAX_PACKET;

    ll_usb_drd_pma_write(PMA_EP0_TX, _dfu.ep0_tx_ptr, len);
    ll_usb_drd_bdt_set_tx_count(0, len);

    _dfu.ep0_tx_ptr += len;
    _dfu.ep0_tx_remain -= len;

    ll_usb_drd_chep_set_stat_tx(0, USB_CHEP_STAT_VALID);
}

static void ep0_send(const uint8_t *data, uint16_t len, uint16_t max_len)
{
    if (len > max_len) len = max_len;
    _dfu.ep0_tx_ptr = data;
    _dfu.ep0_tx_remain = len;
    _dfu.ep0_state = EP0_DATA_IN;
    ep0_tx_packet();
}

static void ep0_send_status(void)
{
    ll_usb_drd_bdt_set_tx_count(0, 0);
    _dfu.ep0_state = EP0_STATUS_IN;
    ll_usb_drd_chep_set_stat_tx(0, USB_CHEP_STAT_VALID);
}

static void ep0_stall(void)
{
    ll_usb_drd_chep_set_stat(0, USB_CHEP_STAT_STALL, USB_CHEP_STAT_STALL);
    _dfu.ep0_state = EP0_IDLE;
}

/* ============================================================
 * DFU flash programming
 * ============================================================ */

static void dfu_program_block(void)
{
    uint32_t addr = APP_ADDRESS + (uint32_t)_dfu.block_num * DFU_TRANSFER_SIZE;

    if (addr < APP_ADDRESS || addr + _dfu.buf_len > APP_END_ADDRESS) {
        _dfu.dfu_status = DFU_STATUS_ERR_ADDRESS;
        _dfu.dfu_state = DFU_STATE_ERROR;
        return;
    }

    /* H5: 8KB sectors. Determine sector number. */
    uint32_t page = (addr - 0x08000000UL) / FLASH_PAGE_SIZE;

    /* Safety: never erase bootloader sector */
    if (page < BOOTLOADER_SECTORS) {
        _dfu.dfu_status = DFU_STATUS_ERR_ADDRESS;
        _dfu.dfu_state = DFU_STATE_ERROR;
        return;
    }

    ll_flash_unlock();

    /* Erase sector if not yet erased */
    if (!((_dfu.erased_pages >> page) & 1)) {
        if (ll_flash_erase_page(page) != 0) {
            ll_flash_lock();
            _dfu.dfu_status = DFU_STATUS_ERR_PROG;
            _dfu.dfu_state = DFU_STATE_ERROR;
            return;
        }
        _dfu.erased_pages |= (1ULL << page);
    }

    /* Program in double-word (8-byte) chunks */
    uint16_t i = 0;
    while (i < _dfu.buf_len) {
        uint32_t word0 = 0xFFFFFFFFUL;
        uint32_t word1 = 0xFFFFFFFFUL;

        for (int b = 0; b < 4 && (i + b) < _dfu.buf_len; b++)
            ((uint8_t *)&word0)[b] = _dfu.buf[i + b];
        for (int b = 0; b < 4 && (i + 4 + b) < _dfu.buf_len; b++)
            ((uint8_t *)&word1)[b] = _dfu.buf[i + 4 + b];

        if (ll_flash_program_dword(addr + i, word0, word1) != 0) {
            ll_flash_lock();
            _dfu.dfu_status = DFU_STATUS_ERR_PROG;
            _dfu.dfu_state = DFU_STATE_ERROR;
            return;
        }

        i += 8;
    }

    ll_flash_lock();

    _dfu.dfu_status = DFU_STATUS_OK;
    _dfu.dfu_state = DFU_STATE_DNLOAD_IDLE;
}

/* ============================================================
 * Setup packet handler
 * ============================================================ */

typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

static void handle_setup(void)
{
    usb_setup_t setup;
    uint8_t buf[8];

    ll_usb_drd_pma_read(PMA_EP0_RX, buf, 8);
    setup.bmRequestType = buf[0];
    setup.bRequest      = buf[1];
    setup.wValue        = buf[2] | (buf[3] << 8);
    setup.wIndex        = buf[4] | (buf[5] << 8);
    setup.wLength       = buf[6] | (buf[7] << 8);

    uint8_t type   = setup.bmRequestType & 0x60;
    uint8_t dir_in = setup.bmRequestType & 0x80;

    /* ---- Standard requests ---- */
    if (type == 0x00) {
        switch (setup.bRequest) {

        case USB_REQ_GET_DESCRIPTOR: {
            uint8_t desc_type = setup.wValue >> 8;
            uint8_t desc_idx  = setup.wValue & 0xFF;

            switch (desc_type) {
            case USB_DESC_DEVICE:
                ep0_send(dev_desc, sizeof(dev_desc), setup.wLength);
                return;
            case USB_DESC_CONFIGURATION:
                ep0_send(cfg_desc, sizeof(cfg_desc), setup.wLength);
                return;
            case USB_DESC_STRING:
                switch (desc_idx) {
                case 0: ep0_send(str0_desc, sizeof(str0_desc), setup.wLength); return;
                case 1: ep0_send(str1_desc, sizeof(str1_desc), setup.wLength); return;
                case 2: ep0_send(str2_desc, sizeof(str2_desc), setup.wLength); return;
                case 3: ep0_send(str3_desc, sizeof(str3_desc), setup.wLength); return;
                case 4: ep0_send(str4_desc, sizeof(str4_desc), setup.wLength); return;
                }
                break;
            case USB_DESC_DEVICE_QUALIFIER:
                ep0_stall();
                return;
            }
            break;
        }

        case USB_REQ_SET_ADDRESS:
            _dfu.address_pending = setup.wValue & 0x7F;
            ep0_send_status();
            return;

        case USB_REQ_SET_CONFIGURATION:
            _dfu.configured = (setup.wValue == 1) ? 1 : 0;
            ep0_send_status();
            return;

        case USB_REQ_GET_CONFIGURATION: {
            static const uint8_t one = 1, zero = 0;
            ep0_send(_dfu.configured ? &one : &zero, 1, setup.wLength);
            return;
        }

        case USB_REQ_GET_STATUS: {
            static const uint8_t status[2] = { 0, 0 };
            ep0_send(status, 2, setup.wLength);
            return;
        }

        case USB_REQ_CLEAR_FEATURE:
        case USB_REQ_SET_FEATURE:
            ep0_send_status();
            return;
        }
    }

    /* ---- DFU class requests ---- */
    if (type == 0x20) {
        switch (setup.bRequest) {

        case DFU_DNLOAD:
            if (setup.wLength > 0) {
                _dfu.block_num = setup.wValue;
                _dfu.buf_len = setup.wLength;
                _dfu.ep0_state = EP0_DATA_OUT;
                ll_usb_drd_chep_set_stat_rx(0, USB_CHEP_STAT_VALID);
            } else {
                _dfu.dfu_state = DFU_STATE_MANIFEST_SYNC;
                ep0_send_status();
            }
            return;

        case DFU_UPLOAD:
            ep0_stall();
            return;

        case DFU_GETSTATUS: {
            if (_dfu.dfu_state == DFU_STATE_DNLOAD_SYNC)
                dfu_program_block();

            if (_dfu.dfu_state == DFU_STATE_MANIFEST_SYNC) {
                _dfu.dfu_state = DFU_STATE_MANIFEST_DONE;
                static uint8_t manifest_resp[6] = {
                    DFU_STATUS_OK, 0, 0, 0,
                    DFU_STATE_IDLE, 0
                };
                ep0_send(manifest_resp, 6, setup.wLength);
                return;
            }

            static uint8_t status_resp[6];
            status_resp[0] = _dfu.dfu_status;
            status_resp[1] = 0;
            status_resp[2] = 0;
            status_resp[3] = 0;
            status_resp[4] = _dfu.dfu_state;
            status_resp[5] = 0;
            ep0_send(status_resp, 6, setup.wLength);
            return;
        }

        case DFU_CLRSTATUS:
            _dfu.dfu_state = DFU_STATE_IDLE;
            _dfu.dfu_status = DFU_STATUS_OK;
            ep0_send_status();
            return;

        case DFU_GETSTATE: {
            static uint8_t state_byte;
            state_byte = _dfu.dfu_state;
            ep0_send(&state_byte, 1, setup.wLength);
            return;
        }

        case DFU_ABORT:
            _dfu.dfu_state = DFU_STATE_IDLE;
            ep0_send_status();
            return;
        }
    }

    if (dir_in) ep0_stall();
    else ep0_send_status();
}

/* ============================================================
 * USB reset handler
 * ============================================================ */

static void handle_reset(void)
{
    ll_usb_drd_chep_config(0, USB_CHEP_CONTROL, 0);
    ll_usb_drd_bdt_set_tx(0, PMA_EP0_TX, 0);
    ll_usb_drd_bdt_set_rx(0, PMA_EP0_RX, EP0_MAX_PACKET);
    ll_usb_drd_chep_set_stat(0, USB_CHEP_STAT_NAK, USB_CHEP_STAT_VALID);
    ll_usb_drd_set_address(0);

    _dfu.configured = 0;
    _dfu.address_pending = 0;
    _dfu.ep0_state = EP0_IDLE;
    _dfu.dfu_state = DFU_STATE_IDLE;
    _dfu.dfu_status = DFU_STATUS_OK;

    USB_CNTR = USB_CNTR_CTRM | USB_CNTR_RESETM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;
}

/* ============================================================
 * System reset helper
 * ============================================================ */

static void system_reset(void) __attribute__((noreturn));
static void system_reset(void)
{
    __asm volatile ("dsb" ::: "memory");
    SCB_AIRCR = (0x05FAUL << 16) | (1UL << 2);
    __asm volatile ("dsb" ::: "memory");
    while (1) ;
}

/* ============================================================
 * Correct Transfer (CTR) handler
 * ============================================================ */

static void handle_ctr(void)
{
    uint32_t istr = USB_ISTR;
    uint8_t  ep   = istr & USB_ISTR_IDN_MASK;

    if (ep != 0) return;

    uint32_t chep0 = ll_usb_drd_chep_read(0);

    /* ---- EP0 RX (OUT/SETUP from host) ---- */
    if (chep0 & USB_CHEP_VTRX) {
        ll_usb_drd_chep_clr_vtrx(0);

        if (chep0 & USB_CHEP_SETUP) {
            handle_setup();
        } else {
            if (_dfu.ep0_state == EP0_DATA_OUT) {
                uint16_t count = ll_usb_drd_bdt_get_rx_count(0);
                if (count <= DFU_TRANSFER_SIZE) {
                    ll_usb_drd_pma_read(PMA_EP0_RX, _dfu.buf, count);
                    _dfu.buf_len = count;
                }
                _dfu.dfu_state = DFU_STATE_DNLOAD_SYNC;
                ep0_send_status();
            } else if (_dfu.ep0_state == EP0_STATUS_OUT) {
                _dfu.ep0_state = EP0_IDLE;
                ll_usb_drd_chep_set_stat_rx(0, USB_CHEP_STAT_VALID);
            } else {
                ll_usb_drd_chep_set_stat_rx(0, USB_CHEP_STAT_VALID);
            }
        }
    }

    /* ---- EP0 TX (IN to host) ---- */
    if (chep0 & USB_CHEP_VTTX) {
        ll_usb_drd_chep_clr_vttx(0);

        if (_dfu.ep0_state == EP0_DATA_IN) {
            if (_dfu.ep0_tx_remain > 0) {
                ep0_tx_packet();
            } else {
                _dfu.ep0_state = EP0_STATUS_OUT;
                ll_usb_drd_chep_set_stat_rx(0, USB_CHEP_STAT_VALID);
            }
        } else if (_dfu.ep0_state == EP0_STATUS_IN) {
            if (_dfu.address_pending) {
                ll_usb_drd_set_address(_dfu.address_pending);
                _dfu.address_pending = 0;
            }

            /* If manifest complete, reset after host has read the status */
            if (_dfu.dfu_state == DFU_STATE_MANIFEST_DONE) {
                for (volatile int i = 0; i < 50000; i++) ;
                ll_usb_drd_disconnect();
                system_reset();
            }

            _dfu.ep0_state = EP0_IDLE;
            ll_usb_drd_chep_set_stat_rx(0, USB_CHEP_STAT_VALID);
        }
    }
}

/* ============================================================
 * USB Interrupt Handler
 * ============================================================ */

void USB_DRD_FS_IRQHandler(void)
{
    uint32_t istr = USB_ISTR;

    if (istr & USB_ISTR_RESET) {
        USB_ISTR = ~USB_ISTR_RESET;
        handle_reset();
        return;
    }

    if (istr & USB_ISTR_CTR) {
        handle_ctr();
    }

    if (istr & USB_ISTR_SUSP) {
        USB_ISTR = ~USB_ISTR_SUSP;
        USB_CNTR |= USB_CNTR_SUSPEN;
    }

    if (istr & USB_ISTR_WKUP) {
        USB_ISTR = ~USB_ISTR_WKUP;
        USB_CNTR &= ~USB_CNTR_SUSPEN;
    }
}

/* ============================================================
 * USB DFU initialization
 * ============================================================ */

static void dfu_usb_init(void)
{
    /* Switch to HSI 32MHz for USB interrupt handling.
     * H5 boots from HSI 64MHz by default. Divide by 2. */
    MOD_BITS(REG32(0x40022000UL), 0xFUL, 1UL);  /* Flash latency 1 */
    while ((REG32(0x40022000UL) & 0xF) != 1) ;
    MOD_BITS(REG32(RCC_BASE + 0x00UL), 3UL << 5, 1UL << 5);  /* HSIDIV=/2 → 32MHz */

    /* Enable HSI48 for crystal-less USB */
    ll_rcc_hsi48_enable();
    while (!ll_rcc_hsi48_ready()) ;

    /* Select HSI48 as USB clock source */
    ll_rcc_set_usb_clk_source(LL_RCC_USB_HSI48);

    /* Enable CRS clock and sync to USB SOF */
    ll_rcc_apb1_clk_enable(LL_APB1_CRS);
    ll_crs_usb_sync_enable();

    /* Enable USB peripheral clock (APB2) */
    ll_rcc_apb2_clk_enable(LL_APB2_USB);

    /* Enable VDDUSB power supply */
    SET_BITS(REG32(PWR_BASE + 0x38UL), (1UL << 24));   /* USB33DEN */
    while (!(REG32(PWR_BASE + 0x3CUL) & (1UL << 24)))  /* Wait USB33RDY */
        ;
    SET_BITS(REG32(PWR_BASE + 0x38UL), (1UL << 25));   /* USB33SV */

    /* Power on USB peripheral */
    ll_usb_drd_power_on();

    /* Enable USB interrupt (IRQ 74) */
    ll_nvic_set_priority(74, 0x30);
    ll_nvic_enable_irq(74);

    /* Initial interrupt mask — RESET only */
    USB_CNTR = USB_CNTR_RESETM;

    /* Connect DP pull-up */
    ll_usb_drd_connect();
}

/* ============================================================
 * Jump to application
 * ============================================================ */

static void jump_to_app(void) __attribute__((noreturn));
static void jump_to_app(void)
{
    uint32_t app_sp    = *(volatile uint32_t *)(APP_ADDRESS);
    uint32_t app_reset = *(volatile uint32_t *)(APP_ADDRESS + 4);

    SCB_VTOR = APP_ADDRESS;

    __asm volatile (
        "msr msp, %0 \n"
        "bx  %1      \n"
        :
        : "r" (app_sp), "r" (app_reset)
        : "memory"
    );

    __builtin_unreachable();
}

/* ============================================================
 * Main — boot decision
 * ============================================================ */

int main(void)
{
    /* Check DFU magic in reserved SRAM (survives system reset) */
    uint32_t magic = DFU_MAGIC_ADDR;

    if (magic == DFU_MAGIC) {
        DFU_MAGIC_ADDR = 0;
        /* Fall through to DFU mode */
    } else {
        /* Check if a valid application is present */
        uint32_t app_sp = *(volatile uint32_t *)(APP_ADDRESS);
        if (app_sp >= SRAM_START && app_sp <= SRAM_END) {
            jump_to_app();
        }
        /* No valid app → fall through to DFU */
    }

    /* ---- Enter DFU mode ---- */
    dfu_usb_init();

    while (1) {
        __asm volatile ("wfi");
    }
}
