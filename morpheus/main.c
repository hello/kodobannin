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

#include "platform.h"

#include <pwm.h>
#include <hrs.h>
#include <watchdog.h>
#include <nrf_sdm.h>
#include <softdevice_handler.h>

#include "app.h"
#include "hble.h"
#include "platform.h"
#include "git_description.h"
#include "morpheus_ble.h"

void
_start()
{

    {
        enum {
            SCHED_QUEUE_SIZE = 20,
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
    APP_OK(softdevice_ant_evt_handler_set(ant_handler));

    // Initialize persistent storage module.
    APP_OK(pstorage_init());

    morpheus_load_modules();

    //hble_init(NRF_CLOCK_LFCLKSRC_SYNTH_250_PPM, true, device_name, hlo_ble_on_ble_evt);
    hble_stack_init();

#ifdef BONDING_REQUIRED   
    hble_bond_manager_init();
#endif

    morpheus_ble_transmission_layer_init();
    
    // append something to device name
    char device_name[strlen(BLE_DEVICE_NAME)+4];
    memcpy(device_name, BLE_DEVICE_NAME, strlen(BLE_DEVICE_NAME));
    uint8_t id = *(uint8_t *)NRF_FICR->DEVICEID;
    device_name[strlen(BLE_DEVICE_NAME)] = '-';
    device_name[strlen(BLE_DEVICE_NAME)+1] = hex[(id >> 4) & 0xF];
    device_name[strlen(BLE_DEVICE_NAME)+2] = hex[(id & 0xF)];
    device_name[strlen(BLE_DEVICE_NAME)+3] = '\0';
    hble_params_init(device_name);

    hlo_ble_init();
    morpheus_ble_services_init();

    ble_uuid_t service_uuid = {
        .type = hello_type,
        .uuid = BLE_UUID_MORPHEUS_SVC
    };

    hble_advertising_init(service_uuid);
    hble_advertising_start();

    for(;;) {
        APP_OK(sd_app_evt_wait());
        app_sched_execute();
    }
}
