#include <app_error.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <nrf_soc.h>
#include <app_gpiote.h>
#include <app_timer.h>
#include <device_params.h>
#include <ble_err.h>
#include <sha.h>
#include <bootloader.h>
#include <dfu_types.h>
#include <bootloader_util_arm.h>
#include <ble_flash.h>
#include <ble_stack_handler.h>
#include <simple_uart.h>
#include <string.h>
#include <spi.h>
#include <util.h>
#include <imu.h>

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

void
test_3v3() {
    nrf_gpio_cfg_output(GPIO_3v3_Enable);
    nrf_gpio_pin_set(GPIO_3v3_Enable);

    nrf_gpio_cfg_output(GPIO_HRS_PWM_G);
    nrf_gpio_cfg_output(GPIO_HRS_PWM_R);
    nrf_gpio_cfg_output(GPIO_VIBE_PWM);

    while (1){
        nrf_gpio_pin_clear(GPIO_HRS_PWM_R);
        nrf_gpio_pin_set  (GPIO_HRS_PWM_G);
        nrf_gpio_pin_set  (GPIO_VIBE_PWM);
        nrf_delay_ms(100);
        nrf_gpio_pin_clear(GPIO_VIBE_PWM);
        nrf_delay_ms(390);
        nrf_gpio_pin_clear(GPIO_HRS_PWM_G);
        nrf_gpio_pin_set  (GPIO_HRS_PWM_R);
        nrf_delay_ms(500);
    } 
}

void
_start()
{
	uint32_t err_code;
    //uint8_t new_fw_sha1[SHA1_DIGEST_LENGTH];
    uint8_t tx[8];
    uint8_t rx[8];
    //APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);

    simple_uart_config(0, 5, 0, 8, false);

    err_code = init_spi(SPI_Channel_0, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);
    APP_ERROR_CHECK(err_code);
	
    //test_3v3();
	//timers_init();
	//ble_init();

	//< hack for when timers are disabled
//	ble_bas_battery_level_update(&m_bas, 69);
	//battery_level_update();

	//application_timers_start();

	//ble_advertising_start();


    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);
    APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);
	//dfu_init();

//	err_code = bootloader_dfu_start();
//	APP_ERROR_CHECK(err_code);
/*
    // GPS Stuff
    nrf_gpio_cfg_output(GPS_ON_OFF);
    nrf_gpio_pin_set(GPS_ON_OFF);
    nrf_delay_ms(2);
    nrf_gpio_pin_clear(GPS_ON_OFF);

    err_code = init_spi(SPI_Channel_1, SPI_Mode1, MISO, MOSI, SCLK, GPS_nCS);
    APP_ERROR_CHECK(err_code);
*/
    err_code = imu_init(SPI_Channel_0);
    APP_ERROR_CHECK(err_code);

// MPU-6500 bit 1 addr: 1 read 0 write, 7 bit addr
    tx[0] = SPI_Read(0x75); //who am i
    tx[1] = 0xff;
    tx[2] = 0xff;
    tx[3] = 0x00;

    while(1) {
        //err_code = spi_xfer(0, IMU_SPI_nCS, 3, tx, rx);
        err_code = spi_xfer(0, IMU_SPI_nCS, 2, tx, rx);
        serial_print_hex((uint8_t *)&err_code, 4);
        simple_uart_putstring((const uint8_t *)" : ");
        serial_print_hex(rx, 2);
        simple_uart_put('\n');
        //serial_print_hex(new_fw_sha1, SHA1_DIGEST_LENGTH);
        //simple_uart_put('\n');
        nrf_delay_ms(300);
    }
	NVIC_SystemReset();
}
