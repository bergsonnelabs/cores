/**
 * hal_spi.c — SPI HAL driver implementation
 */

#include "hal_spi.h"
#include "ll_rcc.h"
#include "ll_dma.h"
#include <string.h>

/* ---- DMA handle lookup for ISR ---- */
#define HAL_SPI_MAX_DMA_INSTANCES  2
static hal_spi_t *_spi_dma_handles[HAL_SPI_MAX_DMA_INSTANCES];

/* ---- Peripheral clock enable ---- */

static void _spi_clk_enable(SPI_TypeDef *instance)
{
    if (instance == SPI1) ll_rcc_apb2_clk_enable(LL_APB2_SPI1);
#if defined(STM32WBA55xx)
    if (instance == SPI3) ll_rcc_apb7_clk_enable(LL_APB7_SPI3);
#elif defined(STM32H523xx)
    if (instance == SPI2) ll_rcc_apb1_clk_enable((1UL << 14));
    if (instance == SPI3) SET_BITS(REG32(RCC_BASE + 0xA8UL), LL_APB3_SPI3);
#endif
    (void)REG32(RCC_BASE);
}

/* ============================================================
 * Init / Deinit
 * ============================================================ */

hal_status_t hal_spi_init(hal_spi_t *h, SPI_TypeDef *instance,
                          const hal_spi_config_t *cfg)
{
    memset(h, 0, sizeof(*h));
    h->instance = instance;
    h->cs_active_low = 1;  /* Default: CS active low */

    _spi_clk_enable(instance);
    ll_spi_init(instance, cfg->prescaler, cfg->cpol, cfg->cpha);
    return HAL_OK;
}

void hal_spi_deinit(hal_spi_t *h)
{
    if (h && h->instance) {
        ll_spi_disable(h->instance);
        h->instance = NULL;
    }
}

void hal_spi_set_cs(hal_spi_t *h, GPIO_TypeDef *port, uint32_t pin)
{
    h->cs_port = port;
    h->cs_pin = pin;

    /* Configure CS as push-pull output and deassert */
    ll_rcc_gpio_clk_enable(port);
    ll_gpio_config_output(port, pin);
    ll_gpio_set(port, 1UL << pin);  /* Deassert (high) */
}

/* ============================================================
 * CS control
 * ============================================================ */

void hal_spi_select(hal_spi_t *h)
{
    if (h->cs_port) {
        if (h->cs_active_low)
            ll_gpio_clear(h->cs_port, 1UL << h->cs_pin);
        else
            ll_gpio_set(h->cs_port, 1UL << h->cs_pin);
    }
}

void hal_spi_deselect(hal_spi_t *h)
{
    if (h->cs_port) {
        if (h->cs_active_low)
            ll_gpio_set(h->cs_port, 1UL << h->cs_pin);
        else
            ll_gpio_clear(h->cs_port, 1UL << h->cs_pin);
    }
}

/* ============================================================
 * Polling transfer
 * ============================================================ */

uint8_t hal_spi_transfer(hal_spi_t *h, uint8_t tx)
{
    return ll_spi_transfer(h->instance, tx);
}

void hal_spi_transfer_buf(hal_spi_t *h, uint8_t *buf, uint32_t len)
{
    ll_spi_transfer_buf(h->instance, buf, len);
}

void hal_spi_write(hal_spi_t *h, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        (void)ll_spi_transfer(h->instance, data[i]);
    }
}

void hal_spi_read(hal_spi_t *h, uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = ll_spi_transfer(h->instance, 0x00);
    }
}

void hal_spi_xfer(hal_spi_t *h, const uint8_t *tx, uint8_t *rx, uint32_t len)
{
    hal_spi_select(h);
    for (uint32_t i = 0; i < len; i++) {
        uint8_t tx_byte = tx ? tx[i] : 0x00;
        uint8_t rx_byte = ll_spi_transfer(h->instance, tx_byte);
        if (rx) rx[i] = rx_byte;
    }
    hal_spi_deselect(h);
}

/* ============================================================
 * DMA transfer
 *
 * Full-duplex DMA for SPI bulk data. Caller manages CS and sends
 * the command/address byte via polling before calling this.
 *
 * DMA channel assignments (per-family):
 *   L422:  SPI1 RX = DMA1_CH2 (request 1), TX = DMA1_CH3 (request 1)
 *   WBA/H5: GPDMA — not yet implemented (SPI v2 needs TSIZE work)
 * ============================================================ */

/* Only the L422 path below wires these into a DMA channel; on the families
 * whose GPDMA support is still outstanding they are dead storage. */
#if defined(STM32L422xx)
/* Dummy byte for TX when tx==NULL (read-only transfer) */
static const uint8_t _spi_dma_dummy_tx = 0x00;

/* Dummy byte for RX when rx==NULL (write-only transfer) */
static uint8_t _spi_dma_dummy_rx;
#endif

hal_status_t hal_spi_xfer_dma(hal_spi_t *h, const uint8_t *tx, uint8_t *rx,
                               uint32_t len, hal_callback_t cb, void *ctx)
{
    if (h->busy) return HAL_BUSY;
    if (len == 0) return HAL_ERROR;

    h->busy          = 1;
    h->complete_cb   = cb;
    h->complete_ctx  = ctx;
    h->dma_rx_buf    = rx;
    h->dma_len       = len;

    /* Register handle for ISR */
    for (int i = 0; i < HAL_SPI_MAX_DMA_INSTANCES; i++) {
        if (_spi_dma_handles[i] == NULL || _spi_dma_handles[i] == h) {
            _spi_dma_handles[i] = h;
            break;
        }
    }

#if defined(STM32L422xx)
    ll_rcc_dma1_clk_enable();

    /* ---- RX channel (DMA1_CH2): SPI DR → memory ---- */
    ll_dma_disable(DMA1_CH2);
    ll_dma_clear_flags(DMA1_CH2);
    ll_dma_set_request(DMA1_CH2, 1);  /* SPI1 = request 1 */

    uint32_t rx_ccr = LL_DMA_CCR_PSIZE_8 | LL_DMA_CCR_MSIZE_8
                    | LL_DMA_CCR_TCIE | LL_DMA_CCR_PL_HIGH;
    if (rx) {
        rx_ccr |= LL_DMA_CCR_MINC;  /* Increment memory pointer */
        ll_dma_config(DMA1_CH2, &h->instance->DR, rx, len, rx_ccr);
    } else {
        /* No RX buffer — dump into dummy byte (no increment) */
        ll_dma_config(DMA1_CH2, &h->instance->DR, &_spi_dma_dummy_rx, len, rx_ccr);
    }

    /* ---- TX channel (DMA1_CH3): memory → SPI DR ---- */
    ll_dma_disable(DMA1_CH3);
    ll_dma_clear_flags(DMA1_CH3);
    ll_dma_set_request(DMA1_CH3, 1);  /* SPI1 = request 1 */

    uint32_t tx_ccr = LL_DMA_CCR_DIR  /* Memory-to-peripheral */
                    | LL_DMA_CCR_PSIZE_8 | LL_DMA_CCR_MSIZE_8
                    | LL_DMA_CCR_PL_MED;
    if (tx) {
        tx_ccr |= LL_DMA_CCR_MINC;
        ll_dma_config(DMA1_CH3, &h->instance->DR, (void *)tx, len, tx_ccr);
    } else {
        /* No TX buffer — send dummy zeros (no increment) */
        ll_dma_config(DMA1_CH3, &h->instance->DR, (void *)&_spi_dma_dummy_tx, len, tx_ccr);
    }

    /* Enable SPI DMA requests */
    ll_spi_enable_dma_rx(h->instance);
    ll_spi_enable_dma_tx(h->instance);

    /* Enable RX DMA interrupt, then start both channels */
    hal_nvic_set_priority(HAL_IRQ_DMA1_CH2, 6);
    hal_nvic_enable_irq(HAL_IRQ_DMA1_CH2);
    ll_dma_enable(DMA1_CH2);  /* RX first */
    ll_dma_enable(DMA1_CH3);  /* TX starts clocking */

    return HAL_OK;

#elif defined(STM32WBA55xx) || defined(STM32H523xx)
    /* GPDMA SPI DMA not yet implemented — SPI v2 requires TSIZE
       to be set while SPE=0, which complicates DMA setup. */
    (void)tx;   /* consumed only by the L422 path above */
    h->busy = 0;
    return HAL_ERROR;

#else
    (void)tx;
    h->busy = 0;
    return HAL_ERROR;
#endif
}

int hal_spi_busy(hal_spi_t *h)
{
    return h->busy;
}

/* ============================================================
 * DMA ISR handlers
 *
 * Override weak startup stubs. RX TC (transfer complete) fires
 * when all data bytes have been received — clean up and callback.
 * ============================================================ */

#if defined(STM32L422xx)

void DMA1_Channel2_IRQHandler(void)
{
    /* RX DMA complete for SPI */
    ll_dma_clear_flags(DMA1_CH2);
    ll_dma_disable(DMA1_CH2);
    ll_dma_disable(DMA1_CH3);

    for (int i = 0; i < HAL_SPI_MAX_DMA_INSTANCES; i++) {
        hal_spi_t *h = _spi_dma_handles[i];
        if (h && h->busy) {
            ll_spi_disable_dma_rx(h->instance);
            ll_spi_disable_dma_tx(h->instance);
            h->busy = 0;
            if (h->complete_cb)
                h->complete_cb(h->complete_ctx);
            break;
        }
    }
}

#endif /* STM32L422xx */
