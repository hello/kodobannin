// vi:sw=4:ts=4

#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <nrf51.h>

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

// we're using a typedef here since this should be opaque to everyone but us
typedef struct {
	NRF_SPI_Type *spi_hw; // the underlying hw reference
	uint8_t cs_gpio;      // which GPIO to use as chip select
} SPI_Context;

enum SPI_Freq {
    Freq_125Kbps = 0,        /*!< drive SClk with frequency 125Kbps */
    Freq_250Kbps,            /*!< drive SClk with frequency 250Kbps */
    Freq_500Kbps,            /*!< drive SClk with frequency 500Kbps */
    Freq_1Mbps,              /*!< drive SClk with frequency 1Mbps */
    Freq_2Mbps,              /*!< drive SClk with frequency 2Mbps */
    Freq_4Mbps,              /*!< drive SClk with frequency 4Mbps */
    Freq_8Mbps               /*!< drive SClk with frequency 8Mbps */
};

/**
 * spi_init - Initialize a hardware SPI block with mode and gpios
 *
 * @param chan - hardware SPI channel to initialize
 * @param mode - one of enum SPI_Mode specifying initial clock state and polarity
 * @param miso - gpio pin to use as MISO signal
 * @param mosi - gpio pin to use as MOSI signal
 * @param sclk - gpio pin to use as SCLK signal
 * @param nCS  - gpio pin to use as active-low chip select signal
 * @param ctx  - pointer to receive SPI_Context opaque data
 *
 * @returns <0 on error, 0 on success
 */
int32_t spi_init(const enum SPI_Channel chan, enum SPI_Mode mode, uint8_t miso, uint8_t mosi, uint8_t sclk, uint8_t nCS, SPI_Context *ctx);

/**
 * spi_xfer - transfer a specified number of bytes to and from a SPI device
 *
 * @param ctx - pointer to SPI_Context opaque data
 * @param tx_len - number of bytes to transmit
 * @param tx - pointer to data to transmit
 * @param rx_len - number of bytes to receive
 * @param rx - pointer to receive data
 *
 * @returns <0 on error, number of bytes transmitted if no receieve specified, otherwise number of bytes received
 */
int32_t spi_xfer(const SPI_Context *ctx, const uint32_t tx_len, const uint8_t *tx, const uint32_t rx_len, uint8_t *rx);

/**
 * spi_command - transfers a command buffer and data buffer to a SPI device and optionally receives a response
 *
 * @param ctx - pointer to SPI_Context opaque data
 * @param cmd_len - number of command bytes to transmit
 * @param cmd - command data to transmit
 * @param tx_len - number of bytes to transmit
 * @param tx - pointer to data to transmit
 * @param rx_len - number of bytes to receive
 * @param rx - pointer to receive data
 *
 * @returns <0 on error, number of bytes transmitted if no receieve specified, otherwise number of bytes received
 */
int32_t spi_command(const SPI_Context *ctx, const uint32_t cmd_len, const uint8_t *cmd_buf, const uint32_t tx_len, const uint8_t *tx, const uint32_t rx_len, uint8_t *rx);

/**
 * spi_destroy - frees up the hardware resources assoiated with a SPI_Context
 *
 * @param ctx - pointer to SPI_Context opqaque reference to hardware to release, will be zeroed on success
 *
 * @returns <0 on error, 0 on success
 */
int32_t spi_destroy(const SPI_Context *ctx);
