// vi:noet:sw=4 ts=4

#include <app_error.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <nrf_soc.h>
#include <app_gpiote.h>
#include <app_timer.h>
#include <ble_err.h>
#include <ble_flash.h>
#include <ble_stack_handler_types.h>
#include <simple_uart.h>
#include <string.h>
#include <spi.h>
#include <spi_nor.h>
#include <util.h>
#include <imu.h>
#include <pwm.h>
#include <hrs.h>
#include <watchdog.h>
#include <hlo_fs.h>
#include <nrf_sdm.h>
#include <softdevice_handler.h>
#include <twi_master.h>

#include "aigz.h"
#include "app.h"
#include "hble.h"
#include "platform.h"
#include "hlo_ble_alpha0.h"
#include "hlo_ble_demo.h"
#include "git_description.h"
#include "pill_ble.h"
#include "sensor_data.h"
#include "util.h"

static bool
_test_rtc()
{
    BOOL_OK(twi_master_init());

    struct aigz_time_t time;

    aigz_read(&time);
    printf("[1/3] RTC test passed: %d/%d/%d%d %d:%d:%d.%d%d\r\n",
           aigz_bcd_decode(time.month),
           aigz_bcd_decode(time.date),
           20+time.century, aigz_bcd_decode(time.year),
           aigz_bcd_decode(time.hours),
           aigz_bcd_decode(time.minutes),
           aigz_bcd_decode(time.seconds),
           aigz_bcd_decode(time.tenths),
           aigz_bcd_decode(time.hundredths));

    return true;
}

static bool
_test_imu()
{
    APP_OK(imu_init(SPI_Channel_1, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS));

    PRINTS("[2/3] IMU test passed.\r\n");

    return true;
}

void
_start()
{
	uint32_t err;

    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);

    APP_TIMER_INIT(APP_TIMER_PRESCALER,
                   APP_TIMER_MAX_TIMERS,
                   APP_TIMER_OP_QUEUE_SIZE,
                   true);
    APP_SCHED_INIT(8, 8);
    APP_GPIOTE_INIT(8);

    BOOL_OK(_test_rtc());
    BOOL_OK(_test_imu());

    // append something to device name
    char device_name[strlen(BLE_DEVICE_NAME)+4];

    memcpy(device_name, BLE_DEVICE_NAME, strlen(BLE_DEVICE_NAME));

    uint8_t id = *(uint8_t *)NRF_FICR->DEVICEID;
    //DEBUG("ID is ", id);
    device_name[strlen(BLE_DEVICE_NAME)] = '-';
    device_name[strlen(BLE_DEVICE_NAME)+1] = hex[(id >> 4) & 0xF];
    device_name[strlen(BLE_DEVICE_NAME)+2] = hex[(id & 0xF)];
    device_name[strlen(BLE_DEVICE_NAME)+3] = '\0';

    hble_init(NRF_CLOCK_LFCLKSRC_SYNTH_250_PPM, false, device_name, hlo_ble_on_ble_evt);

    hlo_ble_init();
    pill_ble_services_init();
    hble_advertising_start();
    PRINTS("[3/3] BLE test passed.\r\n");

    for(;;) {
        app_sched_execute();
        APP_OK(sd_app_evt_wait());
    }
}
