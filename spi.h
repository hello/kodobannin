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

uint32_t init_spi(uint32_t chan, enum SPI_Mode mode, uint8_t miso, uint8_t mosi, uint8_t sclk, uint8_t nCS);
bool spi_xfer(uint32_t chan, uint8_t nCS, uint16_t len, const uint8_t *tx, uint8_t *rx);
