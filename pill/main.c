// vi:noet:sw=4 ts=4

#ifdef ANT_STACK_SUPPORT_REQD
#include <ant_interface.h>
#endif
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

#ifdef PLATFORM_HAS_IMU
#include "message_imu.h"
#endif

#include <watchdog.h>
//#include <hlo_fs.h>
#include <nrf_sdm.h>
#include <softdevice_handler.h>

#include "app.h"
#include "platform.h"

#include "twi_master_config.h"

#include "pill_gatt.h"
#include "hble.h"

#include "pill_ble.h"
#include "sensor_data.h"
#include "util.h"
#include "watchdog.h"

#include "led.h"
#include "battery.h"

#include <twi_master.h>

static void _init_rf_modules()
{
    pill_ble_load_modules();  // MUST load brefore everything else is initialized.

#ifdef ANT_ENABLE
    APP_OK(softdevice_ant_evt_handler_set(ant_handler));
#endif

#ifdef BLE_ENABLE

    hble_stack_init();

#ifdef BONDING_REQUIRED   
    hble_bond_manager_init();
#endif
    // append something to device name
    char device_name[strlen(BLE_DEVICE_NAME)+4];

    memcpy(device_name, BLE_DEVICE_NAME, strlen(BLE_DEVICE_NAME));

    uint8_t id = *(uint8_t *)NRF_FICR->DEVICEID;
    //DEBUG("ID is ", id);
    device_name[strlen(BLE_DEVICE_NAME)] = '-';
    device_name[strlen(BLE_DEVICE_NAME)+1] = hex[(id >> 4) & 0xF];
    device_name[strlen(BLE_DEVICE_NAME)+2] = hex[(id & 0xF)];
    device_name[strlen(BLE_DEVICE_NAME)+3] = '\0';

    hble_params_init(device_name);
    hlo_ble_init();
    pill_ble_services_init();
    PRINTS("pill_ble_init() done\r\n");

    ble_uuid_t service_uuid = {
        .type = hello_type,
        .uuid = BLE_UUID_PILL_SVC
    };

    hble_advertising_init(service_uuid);

    PRINTS("ble_init() done.\r\n");
    hble_update_battery_level();
    hble_advertising_start();
#else
	battery_module_power_on();
	battery_measurement_begin(NULL);
#endif
    PRINTS("INIT DONE.\r\n");
}


static void _load_watchdog()
{
    watchdog_init(10,0);
    watchdog_task_start(5);
}

void _start()
{
    
    battery_module_power_off();

	//HACK TO DISABLE PINS ON LED
#ifdef PLATFORM_HAS_VLED
	gpio_cfg_d0s1_output_disconnect_pull(LED3_ENABLE,NRF_GPIO_PIN_PULLDOWN);
	gpio_cfg_d0s1_output_disconnect_pull(LED2_ENABLE,NRF_GPIO_PIN_PULLDOWN);
	gpio_cfg_d0s1_output_disconnect_pull(LED1_ENABLE,NRF_GPIO_PIN_PULLDOWN);
	gpio_cfg_d0s1_output_disconnect_pull(VRGB_ENABLE,NRF_GPIO_PIN_PULLDOWN);
#endif
	//END HACK
    {
        enum {
            SCHED_QUEUE_SIZE = 32,
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

    _init_rf_modules();
    _load_watchdog();

    for(;;) {
        APP_OK(sd_app_evt_wait());
        app_sched_execute();
    }
}
