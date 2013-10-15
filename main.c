#include <app_error.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <nrf_soc.h>
#include <app_gpiote.h>
#include <app_timer.h>
#include <device_params.h>
#include <ble_err.h>
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
#include <ble_hello_demo.h>
#define APP_GPIOTE_MAX_USERS            2

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

void ble_init();
void ble_advertising_start();

/*
    // GPS Stuff
    nrf_gpio_cfg_output(GPS_ON_OFF);
    nrf_gpio_pin_set(GPS_ON_OFF);
    nrf_delay_ms(2);
    nrf_gpio_pin_clear(GPS_ON_OFF);

    err_code = init_spi(SPI_Channel_1, SPI_Mode1, MISO, MOSI, SCLK, GPS_nCS);
    APP_ERROR_CHECK(err_code);
*/
void
mode_write_handler(ble_gatts_evt_write_t *event) {
    PRINTS("mode_write_handler called\n");
}

void
data_write_handler(ble_gatts_evt_write_t *event) {
    PRINTS("data_write_handler called\n");
}

void
_start()
{
	uint32_t err_code;
    //APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);

    simple_uart_config(0, 5, 0, 8, false);

    err_code = init_spi(SPI_Channel_0, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);
    APP_ERROR_CHECK(err_code);
	
	//timers_init();
	ble_init();
    ble_hello_demo_init_t demo_init = {
            .data_write_handler = &data_write_handler,
            .mode_write_handler = &mode_write_handler,
    };
    err_code = ble_hello_demo_init(&demo_init);
    APP_ERROR_CHECK(err_code);

	//application_timers_start();

	ble_advertising_start();


    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);
    APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);

    err_code = imu_init(SPI_Channel_0);
    APP_ERROR_CHECK(err_code);

    while(1) {
        // Switch to a low power state until an event is available for the application
        err_code = sd_app_event_wait();
        APP_ERROR_CHECK(err_code);
    }

	NVIC_SystemReset();
}
