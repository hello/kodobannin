// vi:noet:sw=4 ts=4

#include <app_gpiote.h>
#include <app_timer.h>
#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <nrf_sdm.h>
#include <softdevice_handler.h>

#include "app.h"
#include "imu.h"
#include "platform.h"
#include "sensor_data.h"
#include "util.h"

//

enum {
    APP_GPIOTE_MAX_USERS = 1,
};

void
_start()
{
    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);

    NRF_CLOCK->TASKS_LFCLKSTART = 1;
    while(NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) {};

    APP_TIMER_INIT(APP_TIMER_PRESCALER,
                   APP_TIMER_MAX_TIMERS,
                   APP_TIMER_OP_QUEUE_SIZE,
                   false);

    APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);

    APP_OK(imu_init(SPI_Channel_1, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS));

    imu_set_wom_callback(imu_printf_wom_callback);
    for(;;) {
        __WFE();

        __SEV();
        __WFE();
    }
}
