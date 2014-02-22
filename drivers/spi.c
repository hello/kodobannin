// vi:sw=4:ts=4

#include <drivers/spi.h>
#include <stdint.h>
#include <nrf51.h>
#include <nrf_gpio.h>

#define TIMEOUT_COUNTER          0x3000UL

int32_t
spi_init(enum SPI_Channel chan, enum SPI_Mode mode, uint8_t miso, uint8_t mosi, uint8_t sclk, uint8_t nCS, SPI_Context *ctx) {
	int32_t ret = -1;

	if (!ctx)
		goto fail;

	// configure SPI context object
    if (chan == SPI_Channel_0)
        ctx->spi_hw = NRF_SPI0;
    else if (chan == SPI_Channel_1)
        ctx->spi_hw = NRF_SPI1;
    else
       	goto fail;

	ctx->cs_gpio = nCS;

    //configure GPIOs
    nrf_gpio_cfg_output(mosi);
    nrf_gpio_cfg_output(sclk);
    nrf_gpio_cfg_output(nCS);
    nrf_gpio_pin_set(nCS);
    nrf_gpio_cfg_input(miso, NRF_GPIO_PIN_NOPULL);

    //configure SPI channel
    ctx->spi_hw->PSELSCK  = sclk;
    ctx->spi_hw->PSELMOSI = mosi;
    ctx->spi_hw->PSELMISO = miso;
    ctx->spi_hw->FREQUENCY =  ( 0x02000000UL << (uint32_t)Freq_1Mbps );

    switch(mode) {
        case SPI_Mode0:
            //MODE0 as per p34 of MPU-6500 v2 0.pdf
            ctx->spi_hw->CONFIG = (SPI_CONFIG_ORDER_MsbFirst << SPI_CONFIG_ORDER_Pos) | (SPI_CONFIG_CPHA_Leading << SPI_CONFIG_CPHA_Pos)  | (SPI_CONFIG_CPOL_ActiveHigh << SPI_CONFIG_CPOL_Pos);
            break;
        case SPI_Mode1:
            ctx->spi_hw->CONFIG = (SPI_CONFIG_ORDER_MsbFirst << SPI_CONFIG_ORDER_Pos) | (SPI_CONFIG_CPHA_Trailing << SPI_CONFIG_CPHA_Pos) | (SPI_CONFIG_CPOL_ActiveHigh << SPI_CONFIG_CPOL_Pos);
            break;
        case SPI_Mode2:
            ctx->spi_hw->CONFIG = (SPI_CONFIG_ORDER_MsbFirst << SPI_CONFIG_ORDER_Pos) | (SPI_CONFIG_CPHA_Leading << SPI_CONFIG_CPHA_Pos)  | (SPI_CONFIG_CPOL_ActiveLow << SPI_CONFIG_CPOL_Pos);
            break;
        case SPI_Mode3:
            ctx->spi_hw->CONFIG = (SPI_CONFIG_ORDER_MsbFirst << SPI_CONFIG_ORDER_Pos) | (SPI_CONFIG_CPHA_Trailing << SPI_CONFIG_CPHA_Pos) | (SPI_CONFIG_CPOL_ActiveLow << SPI_CONFIG_CPOL_Pos);
            break;
        default:
			ret = -2;
			goto fail;
    }

    ctx->spi_hw->EVENTS_READY = 0U;

    ctx->spi_hw->ENABLE = (SPI_ENABLE_ENABLE_Enabled << SPI_ENABLE_ENABLE_Pos);

    return 0;

fail:
	ctx->spi_hw = NULL;
	return ret;
}

static inline bool
spi_one_byte(NRF_SPI_Type *spi, const uint8_t nCS, const uint8_t tx, uint8_t *rx) {
    uint32_t counter = 0;

    spi->TXD = (uint32_t)tx;
    counter = 0;

    /* Wait for the transaction complete or timeout (about 10ms - 20 ms) */
    while ((spi->EVENTS_READY == 0U) && (counter < TIMEOUT_COUNTER))
    {
        ++counter;
    }

    if (counter == TIMEOUT_COUNTER) {
        //we've timed out
        nrf_gpio_pin_set(nCS);
        return false;
    }

    spi->EVENTS_READY = 0U;
    *rx = (uint8_t)spi->RXD;

    return true;
}

static inline bool
_spi_ctx_valid(const SPI_Context *ctx)
{
	if (ctx && ctx->spi_hw)
		return true;

	return false;
}

int32_t
spi_command(const SPI_Context *ctx, const uint32_t cmd_len, const uint8_t *cmd, const uint32_t tx_len, const uint8_t *tx, const uint32_t rx_len, uint8_t *rx)
{
    uint8_t dummy;
    int i;
    int32_t ret = -1;

	if (!_spi_ctx_valid(ctx))
		goto cleanup;

	NRF_SPI_Type *spi = ctx->spi_hw;
	uint8_t nCS = ctx->cs_gpio;

    // select perhipheral
    nrf_gpio_pin_clear(ctx->cs_gpio);

    // send command
    for (i = 0; i < cmd_len; ++i) {
        if (!spi_one_byte(spi, nCS, cmd[i], &dummy)) {
            ret = -2;
            goto cleanup;
        }
    }

	// send data
    for (i = 0; i < tx_len; ++i) {
        if (!spi_one_byte(spi, nCS, tx[i], &dummy)) {
            ret = -2;
            goto cleanup;
        }
    }

    if (rx_len == 0) {
		ret = tx_len;
		goto cleanup;
    }

    // read back data
    dummy = 0;

    for (i = 0; i < rx_len; i++) {
        if (!spi_one_byte(spi, nCS, dummy, &rx[i])) {
            ret = i;
            goto cleanup;
        }
    }

    ret = rx_len;

cleanup:
	nrf_gpio_pin_set(ctx->cs_gpio);
    return ret;
}

int32_t
spi_xfer(const SPI_Context *ctx, const uint32_t tx_len, const uint8_t *tx, const uint32_t rx_len, uint8_t *rx)
{
	return spi_command(ctx, 0, 0, tx_len, tx, rx_len, rx);
}

