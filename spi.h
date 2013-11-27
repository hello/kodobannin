#pragma once
#include <stdint.h>
#include <stdbool.h>

#define SPI_Read(x) (x | 0x80)
#define SPI_Write(x) (x & 0x7F)

enum SPI_Mode {
    SPI_Mode0,
    SPI_Mode1,
    SPI_Mode2,
    SPI_Mode3
};

enum SPI_Channel {
    SPI_Channel_0 = 0,
    SPI_Channel_1 = 1,
    SPI_Channel_Invalid = 0xFF,
};

enum SPI_Freq {
    Freq_125Kbps = 0,        /*!< drive SClk with frequency 125Kbps */
    Freq_250Kbps,            /*!< drive SClk with frequency 250Kbps */
    Freq_500Kbps,            /*!< drive SClk with frequency 500Kbps */
    Freq_1Mbps,              /*!< drive SClk with frequency 1Mbps */
    Freq_2Mbps,              /*!< drive SClk with frequency 2Mbps */
    Freq_4Mbps,              /*!< drive SClk with frequency 4Mbps */
    Freq_8Mbps               /*!< drive SClk with frequency 8Mbps */
};

uint32_t spi_init(const enum SPI_Channel chan, enum SPI_Mode mode, uint8_t miso, uint8_t mosi, uint8_t sclk, uint8_t nCS);
bool     spi_xfer(const enum SPI_Channel chan, const uint8_t nCS, const uint32_t len, const uint8_t *tx, uint8_t *rx);
int32_t  spi_xfer2(const enum SPI_Channel chan, const uint8_t nCS, const uint32_t tx_len, const uint8_t *tx, const uint32_t rx_len, uint8_t *rx);
uint16_t spi_read_multi(const enum SPI_Channel chan, const uint8_t nCS, const uint8_t tx, const uint32_t rxlen, uint8_t *rx);
