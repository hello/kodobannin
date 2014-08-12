// vi:noet:sw=4 ts=4

#include <stddef.h>
#include <string.h>

#include <app_timer.h>
#include <ble_gatts.h>
#include <ble_srv_common.h>
#include <ble_advdata.h>

#include "pill_ble.h"

#ifdef PLATFORM_HAS_IMU

#include "imu.h"
#include "imu_data.h"
#include "sensor_data.h"

#endif

#include "pill_gatt.h"

#include "util.h"
#include "message_app.h"
#include "message_uart.h"
#include "message_ant.h"
#include "message_time.h"
#include "platform.h"
#include "nrf.h"
#include "timedfifo.h"

extern uint8_t hello_type;

static uint16_t _pill_service_handle;
static MSG_Central_t * central;


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


static void
_command_write_handler(ble_gatts_evt_write_t* event)
{
    struct pill_command* command = (struct pill_command*)event->data;

    switch(command->command) {
    case PILL_COMMAND_CALIBRATE:
#ifdef PLATFORM_HAS_IMU
        imu_calibrate_zero();
#endif
        hlo_ble_notify(0xD00D, &command->command, sizeof(command->command), NULL);
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

}


void pill_ble_load_modules(void){
    central = MSG_App_Central(_unhandled_msg_event );
    if(central){
		app_uart_comm_params_t uart_params = {
			SERIAL_RX_PIN,
			SERIAL_TX_PIN,
			SERIAL_RTS_PIN,
			SERIAL_CTS_PIN,
    		APP_UART_FLOW_CONTROL_ENABLED,
			0,
			UART_BAUDRATE_BAUDRATE_Baud38400
		};
		central->loadmod(MSG_App_Base(central));
#ifdef DEBUG_SERIAL
		central->loadmod(MSG_Uart_Base(&uart_params, central));
#endif
		central->loadmod(MSG_Time_Init(central));
#ifdef PLATFORM_HAS_IMU
		central->loadmod(imu_init_base(SPI_Channel_1, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS,central));
#endif		
#ifdef ANT_ENABLE
		central->loadmod(MSG_ANT_Base(central));
#endif
		MSG_SEND_CMD(central, TIME, MSG_TimeCommand_t, TIME_SET_1S_RESOLUTION, NULL, 0);
		MSG_SEND_CMD(central, CENTRAL, MSG_AppCommand_t, APP_LSMOD, NULL, 0);
		{
			uint8_t role = 1;
			MSG_SEND_CMD(central, ANT, MSG_ANTCommand_t, ANT_SET_ROLE, &role, 1);
		}
    }else{
        PRINTS("FAIL");
    }
}

void pill_ble_advertising_init(void){
	ble_advdata_t advdata;
	ble_advdata_t scanrsp;
	uint8_t       flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE; //BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;


	
	ble_uuid_t pill_service_uuid = {
		.type = hello_type,
		.uuid = BLE_UUID_PILL_SVC
	};

	// YOUR_JOB: Use UUIDs for service(s) used in your application.
	ble_uuid_t adv_uuids[] = {pill_service_uuid};
	// Build and set advertising data
	memset(&advdata, 0, sizeof(advdata));
	advdata.name_type = BLE_ADVDATA_FULL_NAME;
	advdata.include_appearance = true;
	advdata.flags.size = sizeof(flags);
	advdata.flags.p_data = &flags;
	


	memset(&scanrsp, 0, sizeof(scanrsp));
	scanrsp.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
	scanrsp.uuids_complete.p_uuids  = adv_uuids;
	APP_OK(ble_advdata_set(&advdata, &scanrsp));
}
