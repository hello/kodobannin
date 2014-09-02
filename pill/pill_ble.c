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
#include "ant_devices.h"
#include "antutil.h"
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
        size_t new_len = len + sizeof(tf_unit_t);
        int32_t aggregate = raw_xyz[0] * raw_xyz[0] + raw_xyz[1] * raw_xyz[1] + raw_xyz[2] * raw_xyz[2];
        aggregate = aggregate >> ((sizeof(aggregate) - sizeof(tf_unit_t)) * 8);
        uint8_t buffer[new_len];
        memcpy(buffer, raw_xyz, len);
        memcpy(&buffer[len], &aggregate, sizeof(tf_unit_t));

		hlo_ble_notify(0xFEED, buffer, new_len, NULL);
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

static void _data_ack_handler(ble_gatts_evt_write_t* event)
{
    PRINTS("_data_ack_handler()\r\n");
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
    hlo_ble_char_notify_add(0xFEED);
    hlo_ble_char_write_command_add(0xF00D, &_data_ack_handler, sizeof(struct pill_data_response));
    hlo_ble_char_notify_add(BLE_UUID_DAY_DATE_TIME_CHAR);
    

}
static void
_send_periodic_data_ant(void * ctx){
	MSG_Data_t * tmp = MSG_Base_AllocateDataAtomic(sizeof(tf_data_condensed_t));
	if(tmp){
		TF_GetCondensed(tmp->buf);
		central->dispatch( (MSG_Address_t){0,0}, (MSG_Address_t){ANT,1}, tmp);
		MSG_Base_ReleaseDataAtomic(tmp);
	}
	/*
	 *static struct{
	 *    uint8_t header;
	 *    uint8_t reserved;
	 *    uint8_t test;
	 *    uint8_t reserved0;
	 *    uint8_t reserved1;
	 *    uint8_t reserved2;
	 *    uint8_t reserved3;
	 *    uint8_t reserved4;
	 *}m;
	 *m.header = 0x66;
	 *MSG_SEND_CMD(central, ANT, MSG_ANTCommand_t, ANT_SEND_RAW, &m, 8);
	 *m.test++;
	 */

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
			uint8_t role = ANT_DISCOVERY_PERIPHERAL;
			MSG_SEND_CMD(central, ANT, MSG_ANTCommand_t, ANT_SET_ROLE, &role, 1);
		}
		{
			ANT_ChannelID_t id = {
				.device_number = GET_UUID_16(),
				.device_type = HLO_ANT_DEVICE_TYPE_PILL_EVT,
				.transmit_type = 0
			};
			MSG_SEND_CMD(central, ANT, MSG_ANTCommand_t, ANT_CREATE_SESSION, &id, sizeof(id));
		}
		{
			app_timer_id_t summary_timer;
			uint32_t ticks;
            ticks = APP_TIMER_TICKS(60000,APP_TIMER_PRESCALER);
			app_timer_create(&summary_timer, APP_TIMER_MODE_REPEATED, _send_periodic_data_ant);
			ticks = app_timer_start(summary_timer, ticks, NULL);
		}
#endif
    }else{
        PRINTS("FAIL");
    }
}
