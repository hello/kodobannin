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
#include <imu.h>
#include <pwm.h>
#include <hrs.h>
#include <watchdog.h>
#include <hlo_fs.h>
#include <nrf_sdm.h>
#include <softdevice_handler.h>
#include <twi_master.h>

#include "app.h"
#include "hble.h"
#include "platform.h"
#include "hlo_ble_alpha0.h"
#include "hlo_ble_demo.h"
#include "git_description.h"
#include "pill_ble.h"
#include "sensor_data.h"
#include "util.h"

void
_start()
{

    //BOOL_OK(twi_master_init());

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

    hble_stack_init(NRF_CLOCK_LFCLKSRC_SYNTH_250_PPM, true);
    hble_bond_manager_init();
    

      // append something to device name
    char device_name[strlen(BLE_DEVICE_NAME)+4];

    memcpy(device_name, BLE_DEVICE_NAME, strlen(BLE_DEVICE_NAME));

    uint8_t id = *(uint8_t *)NRF_FICR->DEVICEID;
    //DEBUG("ID is ", id);
    device_name[strlen(BLE_DEVICE_NAME)] = '-';
    device_name[strlen(BLE_DEVICE_NAME)+1] = hex[(id >> 4) & 0xF];
    device_name[strlen(BLE_DEVICE_NAME)+2] = hex[(id & 0xF)];
    device_name[strlen(BLE_DEVICE_NAME)+3] = '\0';
 
    hble_params_init(device_name, hlo_ble_on_ble_evt);
    pill_ble_load_modules();
    hlo_ble_init();
    pill_ble_services_init();
    PRINTS("pill_ble_init() done\r\n");

    ble_uuid_t service_uuid = {
        .type = hello_type,
        .uuid = BLE_UUID_PILL_SVC
    };
    
    hble_advertising_init(service_uuid);

    PRINTS("ble_init() done.\r\n");

#if 0
    APP_OK(sd_ant_stack_reset());
    PRINTS("ANT initialized.\r\n");
#endif

    
    uint32_t count;
    uint32_t err_code = pstorage_access_status_get(&count);
    if ((err_code == NRF_SUCCESS) && (count == 0))
    {
        
        hble_advertising_start();
        PRINTS("Advertising started.\r\n");
    }

    PRINTS("INIT DONE.\r\n");

    for(;;) {
        APP_OK(sd_app_evt_wait());
        app_sched_execute();
    }
}
