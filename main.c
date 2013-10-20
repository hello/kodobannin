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
#include <pwm.h>
#include <hrs.h>

#define APP_GPIOTE_MAX_USERS            2

static app_timer_id_t imu_sampler;

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

static uint8_t _state;
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
    uint8_t  state = event->data[0];
    uint16_t len = 1;
    uint32_t err;

    DEBUG("mode_write_handler: 0x", state);

    switch (state) {
        case Demo_Config_Standby:
            PRINTS("Enter into sleep mode");
            // TODO: cancel timers and quiesce hardware
            //       schedule maintenance wakeup with RTC
            break;

        case Demo_Config_Calibrating:
            PRINTS("Start HRS Calibration here");
            _state = Demo_Config_Calibrating;
            // TODO: notify HRS system here
            break;

        case Demo_Config_Enter_DFU:
            PRINTS("Rebooting into DFU");
            // set the trap bit for the bootloader and kick the system
            NRF_POWER->GPREGRET |= 0x1;
            NVIC_SystemReset();
            break;

        default:
            DEBUG("Unhandled state transition to 0x", state);
            err = sd_ble_gatts_value_set(event->handle, 0, &len, &_state);
            APP_ERROR_CHECK(err);
    };
}

void
data_write_handler(ble_gatts_evt_write_t *event) {
    PRINTS("data_write_handler called\n");
}

void
cmd_write_handler(ble_gatts_evt_write_t *event) {
    PRINTS("data_write_handler called\n");
}

void
start_sampling_on_connect(void) {
    //TODO: this is the wrong place for this, we must detect when
    //      someone subscribes to the notification instead
    /*
    uint32_t err_code;
    err_code = app_timer_start(imu_sampler, 250, NULL);
    APP_ERROR_CHECK(err_code);
    */
}

void
stop_sampling_on_disconnect(void) {
    //uint32_t err_code;
    //err_code = app_timer_stop(imu_sampler);
    //APP_ERROR_CHECK(err_code);
}

void
sample_imu(void * p_context) {
    static uint8_t counter;
    uint8_t sample[20];
    uint32_t read, sent;

    read = imu_fifo_read(12, &sample[1]);
    if (read == 0)
        return;

    sample[0] = counter++;
    sent = ble_hello_demo_data_send(sample, ++read);
    if (read != sent) {
        DEBUG("Short send of 0x", sent);
    }
    //DEBUG("fifo read bytes: 0x", read);
    //DEBUG("data", sample);
}

static void
hello_demo_service_init() {
    uint32_t err_code;

    // add hello demo service
    ble_hello_demo_init_t demo_init = {
        .data_write_handler = &data_write_handler,
        .mode_write_handler = &mode_write_handler,
        .cmd_write_handler = &cmd_write_handler,
        .conn_handler    = &start_sampling_on_connect,
        .disconn_handler = &stop_sampling_on_disconnect,
    };

    err_code = ble_hello_demo_init(&demo_init);
    APP_ERROR_CHECK(err_code);
}

void
_start()
{
	uint32_t err_code;

    _state = Demo_Config_Standby;

    //pwm_test();

    // setup debug UART
    simple_uart_config(0, 5, 0, 8, false);

    //adc_test();

    // setup timer system
    //APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, false);
    //APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);
/*
    err_code = app_timer_create(&imu_sampler, APP_TIMER_MODE_REPEATED, &sample_imu);
    APP_ERROR_CHECK(err_code);
*/
    hrs_calibrate();
    while(1) {
        __WFE();
    }
    // init ble
	ble_init();

    // init demo app ble service
    hello_demo_service_init();

    // start advertising
	ble_advertising_start();

    // init imu SPI channel and interface
    err_code = init_spi(SPI_Channel_0, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);
    APP_ERROR_CHECK(err_code);

    //imu_selftest(SPI_Channel_0);

    err_code = imu_init(SPI_Channel_0);
    APP_ERROR_CHECK(err_code);

    // start imu sampler - now done on BLE connect
    //err_code = app_timer_start(imu_sampler, 200, NULL);
    //APP_ERROR_CHECK(err_code);

    // loop on BLE events FOREVER
    while(1) {
        // Switch to a low power state until an event is available for the application
        err_code = sd_app_event_wait();
        APP_ERROR_CHECK(err_code);
    }

	NVIC_SystemReset();
}
