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
#include "pstorage.h"

#include "platform.h"

#include <pwm.h>
#include <hrs.h>
#include <watchdog.h>
#include <nrf_sdm.h>
#include <softdevice_handler.h>

#include "app.h"
#include "hble.h"
#include "platform.h"
#include "morpheus_ble.h"

void
_start()
{
#ifdef FACTORY_APP
#include "dtm.h"
	if(NRF_POWER->GPREGRET & GPREGRET_APP_BOOT_TO_DTM){
		NRF_POWER->GPREGRET &= ~GPREGRET_APP_BOOT_TO_DTM;
		sd_softdevice_disable();
		dtm_begin();
	}
#endif

    {
        enum {
            SCHED_QUEUE_SIZE = 16,
            SCHED_MAX_EVENT_DATA_SIZE = MAX(APP_TIMER_SCHED_EVT_SIZE, BLE_STACK_HANDLER_SCHED_EVT_SIZE),
        };

        APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
    }

    APP_TIMER_INIT(APP_TIMER_PRESCALER,
                   APP_TIMER_MAX_TIMERS,
                   APP_TIMER_OP_QUEUE_SIZE,
                   true);

    {
        enum {
            APP_GPIOTE_MAX_USERS = 8,
        };

        APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);
    }

    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, true);

#ifdef ANT_ENABLE
    APP_OK(softdevice_ant_evt_handler_set(ant_handler));
#endif

#ifdef PLATFORM_HAS_PMIC_EN
#include "gpio_nor.h"
	gpio_cfg_d0s1_output_disconnect_pull(PMIC_EN_3V3, NRF_GPIO_PIN_PULLUP);
	gpio_cfg_d0s1_output_disconnect_pull(PMIC_EN_1V8, NRF_GPIO_PIN_PULLUP);
#endif
    // Initialize persistent storage module.
    APP_OK(pstorage_init());
    nrf_delay_ms(100);

#ifdef BONDING_REQUIRED   
    hble_bond_manager_init();
#endif

    morpheus_load_modules();

    //hble_init(NRF_CLOCK_LFCLKSRC_SYNTH_250_PPM, true, device_name, hlo_ble_on_ble_evt);

	watchdog_init(10,0);
	watchdog_task_start(5);

    for(;;) {
        APP_OK(sd_app_evt_wait());
        app_sched_execute();
    }
}
