// vi:noet:sw=4 ts=4

#include <stddef.h>
#include <string.h>

#include <app_timer.h>
#include <ble_gatts.h>
#include <ble_srv_common.h>
#include <ble_advdata.h>

#include "platform.h"

#include "hlo_ble_time.h"
#include "util.h"
#include "message_app.h"
#include "message_sspi.h"
#include "message_uart.h"
#include "message_ant.h"

#include "nrf.h"
#include "morpheus_ble.h"

extern uint8_t hello_type;

static uint16_t _morpheus_service_handle;
static MSG_Central_t * central; 

static void _unhandled_msg_event(void* event_data, uint16_t event_size){
	PRINTS("Unknown Event");
	
}


static void
_data_send_finished()
{
	PRINTS("DONE!");
}

static void _command_write_handler(ble_gatts_evt_write_t* event)
{
/*
 *    struct pill_command* command = (struct pill_command*)event->data;
 *
 *    switch(command->command) {
 *    case PILL_COMMAND_STOP_ACCELEROMETER:
 *        imu_set_wom_callback(NULL);
 *        hlo_ble_notify(0xD00D, &command->command, sizeof(command->command), NULL);
 *        break;
 *    case PILL_COMMAND_START_ACCELEROMETER:
 *        imu_set_wom_callback(_imu_wom_callback);
 *        hlo_ble_notify(0xD00D, &command->command, sizeof(command->command), NULL);
 *        break;
 *    case PILL_COMMAND_CALIBRATE:
 *        imu_calibrate_zero();
 *        hlo_ble_notify(0xD00D, &command->command, sizeof(command->command), NULL);
 *        break;
 *    case PILL_COMMAND_DISCONNECT:
 *        hlo_ble_notify(0xD00D, &command->command, sizeof(command->command), NULL);
 *        break;
 *    case PILL_COMMAND_SEND_DATA:
 *        hlo_ble_notify(0xFEED, TF_GetAll(), TF_GetAll()->length, _data_send_finished);
 *        break;
 *    case PILL_COMMAND_GET_TIME:
 *            {
 *                uint64_t mtime;
 *                if(!MSG_Time_GetMonotonicTime(&mtime)){
 *                    hlo_ble_notify(BLE_UUID_DAY_DATE_TIME_CHAR, &mtime, sizeof(mtime), NULL);
 *                }
 *            }
 *            break;
 *    case PILL_COMMAND_SET_TIME:
 *        {
 *            MSG_SEND(central,TIME,TIME_SYNC,&command->set_time.bytes, sizeof(struct hlo_ble_time));
 *            break;
 *        }
 *    };
 */

	struct morpheus_command* command = (struct morpheus_command*)event->data;
	switch(command->command)
	{
		case MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:
			hble_advertising_start(true);
			break;
		case MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE:
			hble_advertising_start(false);
			break;
	}
}


void morpheus_ble_services_init(void)
{

    {
        ble_uuid_t pill_service_uuid = {
            .type = hello_type,
            .uuid = BLE_UUID_MORPHEUS_SVC ,
        };

        APP_OK(sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &pill_service_uuid, &_morpheus_service_handle));

        hlo_ble_char_write_request_add(0xDEED, &_command_write_handler, sizeof(struct morpheus_command));
        hlo_ble_char_notify_add(0xD00D);
        
        hlo_ble_char_notify_add(BLE_UUID_DAY_DATE_TIME_CHAR);
    }
}

void morpheus_load_modules(void){
    central = MSG_App_Central(_unhandled_msg_event );
    if(central){
    	central->loadmod(MSG_App_Base(central));

#ifdef DEBUG_SERIAL
		app_uart_comm_params_t uart_params = {
			SERIAL_RX_PIN,
			SERIAL_TX_PIN,
			SERIAL_RTS_PIN,
			SERIAL_CTS_PIN,
    		APP_UART_FLOW_CONTROL_DISABLED,
			0,
			UART_BAUDRATE_BAUDRATE_Baud38400
		};

		central->loadmod(MSG_Uart_Base(&uart_params, central));
#endif

#ifdef PLATFORM_HAS_SSPI
		spi_slave_config_t spi_params = {
			//miso
			SSPI_MISO,
			//mosi
			SSPI_MOSI,
			//sck
			SSPI_SCLK,
			//csn
			SSPI_nCS,
			SPI_MODE_0,
			SPIM_MSB_FIRST,
			0xAA,
			0x55,
		};
		central->loadmod(MSG_SSPI_Base(&spi_params,central));
#endif

#ifdef ANT_ENABLE
		central->loadmod(MSG_ANT_Base(central));
#endif
		//MSG_Base_BufferTest();
		MSG_SEND_CMD(central, CENTRAL, MSG_AppCommand_t, APP_LSMOD,NULL,0);
		{
			uint8_t role = 0;
			MSG_SEND_CMD(central, ANT, MSG_ANTCommand_t, ANT_SET_ROLE, &role,1);
		}
    }else{
        PRINTS("FAIL");
    }
}

void morpheus_ble_advertising_init(void){
	ble_advdata_t advdata;
	ble_advdata_t scanrsp;
	uint8_t       flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE; //BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

	ble_uuid_t morpheus_service_uuid = {
		.type = hello_type,
		.uuid = BLE_UUID_MORPHEUS_SVC 
	};

	// YOUR_JOB: Use UUIDs for service(s) used in your application.
	ble_uuid_t adv_uuids[] = {morpheus_service_uuid};

	// Build and set advertising data
	memset(&advdata, 0, sizeof(advdata));
	advdata.name_type = BLE_ADVDATA_FULL_NAME;
	advdata.include_appearance = true;
	advdata.flags.size = sizeof(flags);
	advdata.flags.p_data = &flags;
	


	memset(&scanrsp, 0, sizeof(scanrsp));
	scanrsp.uuids_more_available.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
	scanrsp.uuids_more_available.p_uuids  = adv_uuids;
	APP_OK(ble_advdata_set(&advdata, &scanrsp));
}
