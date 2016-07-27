// vi:noet:sw=4 ts=4

#include <stddef.h>
#include <string.h>
#include <app_timer.h>
#include <ble_gatts.h>
#include <ble_srv_common.h>
#include <ble_advdata.h>
#include "platform.h"
#include "app.h"
#include "pill_gatt.h"
#include "util.h"
#include "message_app.h"
#include "message_uart.h"
#include "pill_ble.h"
#include "message_time.h"
#include "message_led.h"
#include "nrf.h"
#include "timedfifo.h"
#include "cli_user.h"
#include "gpio_nor.h"

#ifdef PLATFORM_HAS_IMU
#include "message_imu.h"
#include "imu_data.h"
#include "sensor_data.h"
#endif

#ifdef ANT_STACK_SUPPORT_REQD
#include <ant_parameters.h>
#endif

#ifdef ANT_ENABLE
#include "ant_user.h"
#include "message_ant.h"
#include "ant_devices.h"
#include "ant_driver.h"
#include "ant_packet.h"
#endif

#include "message_prox.h"

extern uint8_t hello_type;

static uint16_t _pill_service_handle;
static MSG_Central_t * central;


static void
_unhandled_msg_event(void* event_data, uint16_t event_size){
	PRINTS("Unknown Event");
	
}

static void _command_write_handler(ble_gatts_evt_write_t* event)
{
    struct pill_command* command = (struct pill_command*)event->data;

    switch(command->command) {
    case PILL_COMMAND_DISCONNECT:
        hlo_ble_notify(0xD00D, &command->command, sizeof(command->command), NULL);
        break;
	case PILL_COMMAND_WIPE_FIRMWARE:
		REBOOT_TO_DFU();
		break;
#ifdef PLATFORM_HAS_PROX
	case PILL_COMMAND_CALIBRATE:
		//calibrate prox routine
		if(MSG_App_IsModLoaded(PROX)){
			central->dispatch( ADDR(BLE, 0), ADDR(PROX, PROX_START_CALIBRATE), NULL);
		} else {//try load it again
#ifdef PLATFORM_HAS_PROX
			central->loadmod(MSG_Prox_Init(central));
#endif
			if(MSG_App_IsModLoaded(PROX)){
				central->dispatch( ADDR(BLE, 0), ADDR(PROX, PROX_START_CALIBRATE), NULL);
			}else{
				hlo_ble_notify(0xD00D, "I2CFail", 7, NULL);
			}
		}
		break;
	case PILL_COMMAND_WIPE_CALIBRATION:
		central->dispatch( ADDR(BLE, 0), ADDR(PROX, PROX_ERASE_CALIBRATE), NULL);
		break;
#endif
	case PILL_COMMAND_RESET:
		REBOOT();
		break;
	case PILL_COMMAND_READ_PROX:
		central->dispatch( ADDR(BLE, 0), ADDR(PROX, PROX_READ_REPLY_BLE), NULL);
		break;
    default:
        break;
    };
}

void pill_ble_evt_handler(ble_evt_t* ble_evt)
{
    PRINTS("Pill BLE event handler: ");
    PRINT_HEX(&ble_evt->header.evt_id, sizeof(ble_evt->header.evt_id));
    PRINTS("\r\n");
    // sd_ble_gatts_rw_authorize_reply
}

void
pill_ble_services_init(void)
{
    ble_uuid_t pill_service_uuid = {
        .type = hello_type,
        .uuid = BLE_UUID_PILL_SVC,
    };

    APP_OK(sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &pill_service_uuid, &_pill_service_handle));

    hlo_ble_char_write_request_add(0xDEED, &_command_write_handler, sizeof(struct pill_command));
    hlo_ble_char_notify_add(0xD00D);
}

int is_debug_enabled(){
#ifdef DEBUG_SERIAL
	return 1;
#endif
#ifdef PLATFORM_HAS_SERIAL
	uint32_t val = nrf_gpio_pin_read(SERIAL_RX_PIN);
	gpio_cfg_d0s1_output_disconnect(SERIAL_RX_PIN);
	return !val;
#endif
	return 0;
}

void pill_ble_load_modules(void){
    central = MSG_App_Central(_unhandled_msg_event );
    if(central){
		central->loadmod(MSG_App_Base(central));
		if(is_debug_enabled()){
			app_uart_comm_params_t uart_params = {
				SERIAL_RX_PIN,
				SERIAL_TX_PIN,
				SERIAL_RTS_PIN,
				SERIAL_CTS_PIN,
				APP_UART_FLOW_CONTROL_ENABLED,
				0,
				UART_BAUDRATE_BAUDRATE_Baud38400
			};
			central->loadmod(MSG_Uart_Base(&uart_params, central));
			central->loadmod(MSG_Cli_Base(central, Cli_User_Init(central, NULL)));
		}
		central->loadmod(MSG_Time_Init(central));
#ifdef PLATFORM_HAS_IMU
		central->loadmod(MSG_IMU_Init(central));
#endif

#ifdef ANT_ENABLE
#ifdef PILL_ANT_TYPE
        central->loadmod(MSG_ANT_Base(central, ANT_UserInit(central), HLO_ANT_ROLE_PERIPHERAL, PILL_ANT_TYPE));
#else
        central->loadmod(MSG_ANT_Base(central, ANT_UserInit(central), HLO_ANT_ROLE_PERIPHERAL, HLO_ANT_DEVICE_TYPE_PILL));
#endif

#endif

        central->dispatch( ADDR(CENTRAL, 0), ADDR(TIME, MSG_TIME_SET_START_1SEC), NULL);
        central->dispatch( ADDR(CENTRAL, 0), ADDR(TIME, MSG_TIME_SET_START_1MIN), NULL);

#ifdef PLATFORM_HAS_VLED
  		central->loadmod(MSG_LEDInit(central));
#endif

#ifdef PLATFORM_HAS_PROX
		central->loadmod(MSG_Prox_Init(central));
#endif

		central->dispatch( ADDR(CENTRAL, 0), ADDR(CENTRAL,MSG_APP_LSMOD), NULL);
    }else{
        PRINTS("FAIL");
    }
}
