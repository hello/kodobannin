#include <app_error.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <nrf_soc.h>
#include <app_timer.h>
#include <device_params.h>
#include <ble_err.h>
#include "sha.h"
#include <bootloader.h>
#include <dfu_types.h>
#include <bootloader_util_arm.h>
#include <ble_flash.h>
#include <ble_stack_handler.h>
#include <simple_uart.h>
#include <string.h>
#include <spi_master.h>

#define APP_GPIOTE_MAX_USERS            2

static void
sha1_fw_area(uint8_t *hash)
{
	SHA_CTX ctx;
	uint32_t code_size    = DFU_IMAGE_MAX_SIZE_FULL;
	uint8_t *code_address = (uint8_t *)CODE_REGION_1_START;
	uint32_t *index = (uint32_t *)BOOTLOADER_REGION_START - DFU_APP_DATA_RESERVED - 4;

	// walk back to the end of the actual firmware
	while (*index-- == EMPTY_FLASH_MASK && code_size > 0)
		code_size -= 4;

    // only measure if there is something to measure
    memset(hash, 0, SHA1_DIGEST_LENGTH);
    if (code_size ==  0)
        return;

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, (void *)code_address, code_size);
	SHA1_Final(hash, &ctx);
}

static bool
verify_fw_sha1(uint8_t *valid_hash)
{
	uint8_t sha1[SHA1_DIGEST_LENGTH];
    uint8_t comp = 0;
    int i = 0;

    sha1_fw_area(sha1);

    for (i = 0; i < SHA1_DIGEST_LENGTH; i++)
        comp |= sha1[i] ^ valid_hash[i];

    return comp == 0;
}

uint32_t
init_spi(uint32_t chan, uint8_t miso, uint8_t mosi, uint8_t sclk, uint8_t nCS) {
    NRF_SPI_Type *spi;

    if (chan == 0)
        spi = NRF_SPI0;
    else if (chan == 1)
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

    //MODE0 as per p34 of MPU-6500 v2 0.pdf 
    spi->CONFIG = (SPI_CONFIG_ORDER_MsbFirst << SPI_CONFIG_ORDER_Pos) | (SPI_CONFIG_CPHA_Leading << SPI_CONFIG_CPHA_Pos) | (SPI_CONFIG_CPOL_ActiveHigh << SPI_CONFIG_CPOL_Pos);
    //(SPI_CONFIG_CPHA_Leading << SPI_CONFIG_CPHA_Pos) | (SPI_CONFIG_CPOL_ActiveHigh << SPI_CONFIG_CPOL_Pos);//
    
    spi->EVENTS_READY = 0U;

    spi->ENABLE = (SPI_ENABLE_ENABLE_Enabled << SPI_ENABLE_ENABLE_Pos);

    return 0;
}

#define TIMEOUT_COUNTER          0x3000UL

bool
spi_xfer(uint32_t chan, uint8_t nCS, uint16_t len, const uint8_t *tx, uint8_t *rx) {
    NRF_SPI_Type *spi;
    uint32_t counter;
    int i;

    if (chan == 0)
        spi = NRF_SPI0;
    else if (chan == 1)
        spi = NRF_SPI1;
    else
        return false;    

    // select perhipheral
    nrf_gpio_pin_clear(nCS);

    for (i = 0; i < len; i++) {
        spi->TXD = (uint32_t)tx[i];
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
        rx[i] = (uint8_t)spi->RXD;
    }

    nrf_gpio_pin_set(nCS);
    return true;
}

void
_start()
{
	uint32_t err_code;
    uint8_t new_fw_sha1[SHA1_DIGEST_LENGTH];
    uint8_t tx[8];
    uint8_t rx[8];
    //APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);

    simple_uart_config(0, 30, 0, 3, false);

    err_code = init_spi(0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);
    APP_ERROR_CHECK(err_code);


    err_code = init_spi(1, MISO, MOSI, SCLK, FLASH_nCS);
    APP_ERROR_CHECK(err_code);

// MPU-6500 bit 1 addr: 1 read 0 write, 7 bit addr
    tx[0] = 0x90;//0x75 | 0x80; //who am i
    tx[1] = 0xff;
    tx[2] = 0xff;
    tx[3] = 0x00;

    while(1) {
        //err_code = spi_xfer(0, IMU_SPI_nCS, 3, tx, rx);
        err_code = spi_xfer(1, FLASH_nCS, 6, tx, rx);
        serial_print_hex(&err_code, 4);
        simple_uart_putstring(" : ");
    serial_print_hex(rx, 6);
        simple_uart_put('\n');
        //serial_print_hex(new_fw_sha1, SHA1_DIGEST_LENGTH);
        //simple_uart_put('\n');
        nrf_delay_ms(500);
    }
}
