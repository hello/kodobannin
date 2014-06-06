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
#include <drivers/spi.h>
#include <drivers/spi_nor.h>
#include <util.h>
#include <imu.h>
#include <drivers/pwm.h>
#include <hrs.h>
#include <drivers/watchdog.h>
#include <drivers/hlo_fs.h>
#include <nrf_sdm.h>
#include <softdevice_handler.h>
#include <twi_master.h>

#include "hble.h"
#include "platform.h"
#include "hlo_ble_alpha0.h"
#include "hlo_ble_demo.h"
#include "git_description.h"
#include "sensor_data.h"
#include "util.h"

enum {
    APP_TIMER_PRESCALER = 0,
    APP_TIMER_MAX_TIMERS = 4,
    APP_TIMER_OP_QUEUE_SIZE = 5,
};

static const char* const _DEVICE_NAME = "AndreTest";

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

    hble_init(NRF_CLOCK_LFCLKSRC_SYNTH_250_PPM, false, _DEVICE_NAME, NULL);
    PRINTS("ble_init() done.\r\n");

    hlo_ble_demo_init_t demo_init = {
        .conn_handler    = NULL,
        .disconn_handler = NULL,
        .mode_write_handler = NULL,
        .cmd_write_handler = NULL,
    };

    hlo_ble_init();
    hlo_ble_demo_init(&demo_init);
    PRINTS("demo_init done\r\n");

    hble_advertising_start();
    PRINTS("Advertising started.\r\n");

    for(;;) {
        app_sched_execute();
        APP_OK(sd_app_evt_wait());
    }

    // NVIC_SystemReset();
}
