// vi:noet:sw=4 ts=4

#include <stddef.h>
#include <string.h>

#include <app_timer.h>
#include <ble_gatts.h>
#include <ble_srv_common.h>
#include <ble_advdata.h>

#include "pill_ble.h"

#ifdef PLATFORM_HAS_IMU

#include "message_imu.h"
#include "imu_data.h"
#include "sensor_data.h"

#endif

#include "pill_gatt.h"

#include "util.h"
#include "message_app.h"
#include "message_uart.h"

#ifdef ANT_ENABLE
#include "message_ant.h"
#endif

#include "message_time.h"
#include "platform.h"
#include "nrf.h"
#include "timedfifo.h"


extern uint8_t hello_type;

static uint16_t _pill_service_handle;
static MSG_Central_t * central;
static bool _should_stream = false;

static void
_unhandled_msg_event(void* event_data, uint16_t event_size){
	PRINTS("Unknown Event");
	
}


static void
_data_send_finished()
{
	PRINTS("DONE!");
}

static void _reply_time(void * p_event_data, uint16_t event_size)
{
	struct hlo_ble_time* current_time = get_time();
	if(current_time)
	{
		hlo_ble_notify(BLE_UUID_DAY_DATE_TIME_CHAR, current_time, sizeof(struct hlo_ble_time), NULL);
	}
}


static void _on_motion_data_arrival(const int16_t* raw_xyz, size_t len)
{
	if(_should_stream)
	{
		hlo_ble_notify(0xFEED, raw_xyz, len, NULL);
	}
	
}



static void _calibrate_imu(void * p_event_data, uint16_t event_size)
{
#ifdef PLATFORM_HAS_IMU
        imu_calibrate_zero();
#endif
        struct pill_command* command = (struct pill_command*)p_event_data;
        hlo_ble_notify(0xD00D, &command->command, sizeof(command->command), NULL);
}


static void _command_write_handler(ble_gatts_evt_write_t* event)
{
    struct pill_command* command = (struct pill_command*)event->data;

    switch(command->command) {
    case PILL_COMMAND_CALIBRATE:
    	app_sched_event_put(command, event->len, _calibrate_imu);
        break;
    case PILL_COMMAND_DISCONNECT:
        hlo_ble_notify(0xD00D, &command->command, sizeof(command->command), NULL);
        break;
    case PILL_COMMAND_SEND_DATA:
        //hlo_ble_notify(0xFEED, _daily_data, sizeof(_daily_data), _data_send_finished);
		hlo_ble_notify(0xFEED, TF_GetAll(), TF_GetAll()->length, _data_send_finished);
        break;
    case PILL_COMMAND_GET_TIME:
			/*{
				uint64_t mtime;
				if(!MSG_Time_GetMonotonicTime(&mtime)){
    				hlo_ble_notify(BLE_UUID_DAY_DATE_TIME_CHAR, &mtime, sizeof(mtime), NULL);
				}
			}*/
			/*if(!MSG_Time_GetTime(&_current_time)){
    			hlo_ble_notify(BLE_UUID_DAY_DATE_TIME_CHAR, _current_time.bytes, sizeof(_current_time), NULL);
			}*/
			app_sched_event_put(NULL, 0, _reply_time);
            break;
    case PILL_COMMAND_SET_TIME:
        {
			MSG_SEND_CMD(central,TIME,MSG_TimeCommand_t, TIME_SYNC, &command->set_time.bytes, sizeof(struct hlo_ble_time));
            break;
        }
    case PILL_COMMAND_START_ACCELEROMETER:
    	PRINTS("Streamming started\r\n");
    	_should_stream = true;
    	imu_set_wom_callback(_on_motion_data_arrival);
    	break;
    case PILL_COMMAND_STOP_ACCELEROMETER:
    	_should_stream = false;
    	imu_set_wom_callback(NULL);
    	PRINTS("Streamming stopped\r\n");
    	break;
	case PILL_COMMAND_GET_BATTERY_LEVEL:
#ifdef DEBUG_BATT_LVL
		hble_update_battery_level();
#endif
		break;
    };
}

static void
_data_ack_handler(ble_gatts_evt_write_t* event)
{
    PRINTS("_data_ack_handler()\r\n");
}

void pill_ble_evt_handler(ble_evt_t* ble_evt)
{
    PRINTS("Pill BLE event handler: ");
    PRINT_HEX(ble_evt->header.evt_id, sizeof(ble_evt->header.evt_id));
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
    hlo_ble_char_notify_add(0xFEED);
    hlo_ble_char_write_command_add(0xF00D, &_data_ack_handler, sizeof(struct pill_data_response));
    hlo_ble_char_notify_add(BLE_UUID_DAY_DATE_TIME_CHAR);
    

}


void pill_ble_load_modules(void){
    central = MSG_App_Central(_unhandled_msg_event );
    if(central){
		central->loadmod(MSG_App_Base(central));
#ifdef DEBUG_SERIAL
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
#endif
		central->loadmod(MSG_Time_Init(central));
#ifdef PLATFORM_HAS_IMU
		central->loadmod(MSG_IMU_Init(central));
#endif

#ifdef ANT_ENABLE
		central->loadmod(MSG_ANT_Base(central));
#endif
		MSG_SEND_CMD(central, TIME, MSG_TimeCommand_t, TIME_SET_1S_RESOLUTION, NULL, 0);
		MSG_SEND_CMD(central, CENTRAL, MSG_AppCommand_t, APP_LSMOD, NULL, 0);
#ifdef ANT_ENABLE
		{
			uint8_t role = 1;
			MSG_SEND_CMD(central, ANT, MSG_ANTCommand_t, ANT_SET_ROLE, &role, 1);
		}
		{
/*
 *            ANT_Channel_Settings_t s = (ANT_Channel_Settings_t){
 *                .phy = (ANT_ChannelPHY_t){
 *                    .channel_type = 0x10,
 *                    .frequency = 66,
 *                    .network = 0,
 *                    .period = 1092
 *                },
 *                .id = (ANT_ChannelID_t){
 *                    .device_number = 0xABCD,
 *                    .device_type = 0x11,
 *                    .transmit_type = 0
 *
 *                }
 *            };
 *            MSG_SEND_CMD(central, ANT, MSG_ANTCommand_t, ANT_CREATE_SESSION, &s, sizeof(s));
 */
		}
#endif
    }else{
        PRINTS("FAIL");
    }
}
