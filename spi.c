#include <spi.h>
#include <stdint.h>
#include <nrf51.h>
#include <nrf_gpio.h>

#define TIMEOUT_COUNTER          0x3000UL

uint32_t
init_spi(uint32_t chan, enum SPI_Mode mode, uint8_t miso, uint8_t mosi, uint8_t sclk, uint8_t nCS) {
    NRF_SPI_Type *spi;

    if (chan == SPI_Channel_0)
        spi = NRF_SPI0;
    else if (chan == SPI_Channel_1)
        spi = NRF_SPI1;
    else
        return -1;

    //configure GPIOs
    nrf_gpio_cfg_output(mosi); 
    nrf_gpio_cfg_output(sclk);
    nrf_gpio_cfg_output(nCS);
    nrf_gpio_pin_set(nCS);
    nrf_gpio_cfg_input(miso, NRF_GPIO_PIN_NOPULL);

    //configure SPI channel
    spi->PSELSCK  = sclk;
    spi->PSELMOSI = mosi;
    spi->PSELMISO = miso;
    spi->FREQUENCY =  ( 0x02000000UL << (uint32_t)Freq_1Mbps );

    switch(mode) {
        case SPI_Mode0:
            //MODE0 as per p34 of MPU-6500 v2 0.pdf 
            spi->CONFIG = (SPI_CONFIG_ORDER_MsbFirst << SPI_CONFIG_ORDER_Pos) | (SPI_CONFIG_CPHA_Leading << SPI_CONFIG_CPHA_Pos)  | (SPI_CONFIG_CPOL_ActiveHigh << SPI_CONFIG_CPOL_Pos);
            break;
        case SPI_Mode1:
            spi->CONFIG = (SPI_CONFIG_ORDER_MsbFirst << SPI_CONFIG_ORDER_Pos) | (SPI_CONFIG_CPHA_Trailing << SPI_CONFIG_CPHA_Pos) | (SPI_CONFIG_CPOL_ActiveHigh << SPI_CONFIG_CPOL_Pos);
            break;
        case SPI_Mode2:
            spi->CONFIG = (SPI_CONFIG_ORDER_MsbFirst << SPI_CONFIG_ORDER_Pos) | (SPI_CONFIG_CPHA_Leading << SPI_CONFIG_CPHA_Pos)  | (SPI_CONFIG_CPOL_ActiveLow << SPI_CONFIG_CPOL_Pos);
            break;
        case SPI_Mode3:
            spi->CONFIG = (SPI_CONFIG_ORDER_MsbFirst << SPI_CONFIG_ORDER_Pos) | (SPI_CONFIG_CPHA_Trailing << SPI_CONFIG_CPHA_Pos) | (SPI_CONFIG_CPOL_ActiveLow << SPI_CONFIG_CPOL_Pos);
            break;
        default:
            return -2;
    }

    spi->EVENTS_READY = 0U;

    spi->ENABLE = (SPI_ENABLE_ENABLE_Enabled << SPI_ENABLE_ENABLE_Pos);

    return 0;
}

static bool
spi_one_byte(NRF_SPI_Type *spi, const uint8_t nCS, const uint8_t tx, uint8_t *rx) {
    uint32_t counter = 0;

    spi->TXD = (uint32_t)tx;
    counter = 0;

    /* Wait for the transaction complete or timeout (about 10ms - 20 ms) */
    while ((spi->EVENTS_READY == 0U) && (counter < TIMEOUT_COUNTER))
    {
        counter++;
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

bool
spi_xfer(const enum SPI_Channel chan, const uint8_t nCS, const uint32_t len, const uint8_t *tx, uint8_t *rx) {
    NRF_SPI_Type *spi;
    int i;

    if (chan == SPI_Channel_0)
        spi = NRF_SPI0;
    else if (chan == SPI_Channel_1)
        spi = NRF_SPI1;
    else
        return false;    

    // select perhipheral
    nrf_gpio_pin_clear(nCS);

    for (i = 0; i < len; i++) {
        if (!spi_one_byte(spi, nCS, tx[i], &rx[i]))
            return false;
    }

    nrf_gpio_pin_set(nCS);
    return true;
}

uint16_t
spi_read_multi(const enum SPI_Channel chan, const uint8_t nCS, const uint8_t tx, const uint32_t rxlen, uint8_t *rx) {
    NRF_SPI_Type *spi;
    int i;

    if (chan == SPI_Channel_0)
        spi = NRF_SPI0;
    else if (chan == SPI_Channel_1)
        spi = NRF_SPI1;
    else
        return 0;

    // select perhipheral
    nrf_gpio_pin_clear(nCS);

    if (!spi_one_byte(spi, nCS, tx, rx))
            return 0;

    for (i = 0; i < rxlen; i++) {
        if (!spi_one_byte(spi, nCS, 0xFF, &rx[i]))
            return i;
    }

    return rxlen;
}
