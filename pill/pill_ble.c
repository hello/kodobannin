// vi:noet:sw=4 ts=4

#include <stddef.h>
#include <string.h>

#include <app_timer.h>
#include <ble_gatts.h>
#include <ble_srv_common.h>
#include <ble_advdata.h>

#include "imu_data.h"
#include "sensor_data.h"
#include "imu.h"
#include "rtc.h"
#include "hlo_ble.h"
#include "hlo_ble_time.h"
#include "pill_ble.h"
#include "util.h"
#include "message_app.h"
#include "message_uart.h"
#include "message_time.h"
#include "platform.h"
#include "nrf.h"
#include "timedfifo.h"

extern uint8_t hello_type;

static uint16_t _pill_service_handle;
static uint16_t _stream_service_handle;
static MSG_Central_t * central; 
//static uint8_t _daily_data[1440];

static uint8_t _imu_realtime_stream_data[12];

static void
_unhandled_msg_event(void* event_data, uint16_t event_size){
	PRINTS("Unknown Event");
	
}
static void
_send_imu_realtime_stream_data(void* imu_data, uint16_t fifo_bytes_available)
{
    hlo_ble_notify(0xFFAA, imu_data, fifo_bytes_available, NULL);
}

static void
_imu_data_ready(uint16_t fifo_bytes_available)
{
    fifo_bytes_available = MIN(12, fifo_bytes_available);
    imu_fifo_read(fifo_bytes_available, _imu_realtime_stream_data);
    app_sched_event_put(_imu_realtime_stream_data, fifo_bytes_available, _send_imu_realtime_stream_data);
}

enum aggregation_method {
    AGGREGATE_SAMPLES_MAX = 0,
    AGGREGATE_SAMPLES_AVERAGE = 1,
};

static void
_aggregate_samples(uint8_t* data, uint16_t size, enum aggregation_method method)
{
    /* ~k/build/pill+pill_EVT1 % echo $(( (255**2+255**2+255**2) >> 10)) */
}

static void
_imu_wom_callback(struct sensor_data_header* header)
{
    enum {
        MAX_FIFO_READ_SIZE = 180, // 30 samples worth of accelerometer data
    };

    unsigned read_size = MIN(header->size, MAX_FIFO_READ_SIZE);

    uint8_t data[read_size];
    imu_fifo_read(read_size, data);

    _aggregate_samples(data, read_size, AGGREGATE_SAMPLES_MAX);
}

static void
_data_send_finished()
{
	PRINTS("DONE!");
}

static void
_stream_write_handler(ble_gatts_evt_write_t* event)
{
    struct pill_stream_command* command = (struct pill_stream_command*)event->data;

    switch(command->command) {
    case PILL_STREAM_COMMAND_STOP:
        imu_set_data_ready_callback(NULL);
        imu_set_wom_callback(_imu_wom_callback);
        break;
    case PILL_STREAM_COMMAND_START:
        imu_set_wom_callback(NULL);
        imu_activate();
        imu_set_data_ready_callback(_imu_data_ready);
        break;
    }
}


static void
_command_write_handler(ble_gatts_evt_write_t* event)
{
    struct pill_command* command = (struct pill_command*)event->data;

    switch(command->command) {
    case PILL_COMMAND_STOP_ACCELEROMETER:
        imu_set_wom_callback(NULL);
        hlo_ble_notify(0xD00D, &command->command, sizeof(command->command), NULL);
        break;
    case PILL_COMMAND_START_ACCELEROMETER:
        imu_set_wom_callback(_imu_wom_callback);
        hlo_ble_notify(0xD00D, &command->command, sizeof(command->command), NULL);
        break;
    case PILL_COMMAND_CALIBRATE:
        imu_calibrate_zero();
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
			{
				uint64_t mtime;
				if(!MSG_Time_GetMonotonicTime(&mtime)){
    				hlo_ble_notify(BLE_UUID_DAY_DATE_TIME_CHAR, &mtime, sizeof(mtime), NULL);
				}
			}
			/*if(!MSG_Time_GetTime(&_current_time)){
    			hlo_ble_notify(BLE_UUID_DAY_DATE_TIME_CHAR, _current_time.bytes, sizeof(_current_time), NULL);
			}*/
            break;
    case PILL_COMMAND_SET_TIME:
        {
			MSG_SEND(central,TIME,TIME_SYNC,&command->set_time.bytes, sizeof(struct hlo_ble_time));
            break;
        }
    };
}

static void
_data_ack_handler(ble_gatts_evt_write_t* event)
{
    PRINTS("_data_ack_handler()\r\n");
}

void
pill_ble_evt_handler(ble_evt_t* ble_evt)
{
    DEBUG("Pill BLE event handler: ", ble_evt->header.evt_id);
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

    {
        ble_uuid_t stream_service_uuid = {
            .type = hello_type,
            .uuid = 0xFFA0,
        };

        APP_OK(sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &stream_service_uuid, &_stream_service_handle));

        hlo_ble_char_write_command_add(0xFFA1, &_stream_write_handler, 1);
        hlo_ble_char_notify_add(0xFFAA);
    }
}

void
pill_ble_load_modules(void){
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
		central->loadmod(MSG_Uart_Base(&uart_params, central));
		central->loadmod(MSG_Time_Init(central));
		central->loadmod(imu_init_base(SPI_Channel_1, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS,central));
		MSG_SEND(central, TIME, TIME_SET_1S_RESOLUTION,NULL,0);
		MSG_SEND(central, CENTRAL, APP_LSMOD,NULL,0);
    }else{
        PRINTS("FAIL");
    }
}
void pill_ble_advertising_init(void){
	ble_advdata_t advdata;
	ble_advdata_t scanrsp;
	uint8_t       flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE; //BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;


	uint8_t array[] = { 100 };  // get battery level here?

	uint8_array_t data = {
		.size = sizeof(array),
		.p_data = array
	};

	ble_advdata_service_data_t battery_service_data = {
		.service_uuid = BLE_UUID_BATTERY_SERVICE,
		.data = data
	};

	ble_advdata_service_data_t service_data_array[] = { battery_service_data };

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
	advdata.p_service_data_array = service_data_array;
	advdata.service_data_count = sizeof(service_data_array) / sizeof(service_data_array[0]);




	memset(&scanrsp, 0, sizeof(scanrsp));
	scanrsp.uuids_more_available.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
	scanrsp.uuids_more_available.p_uuids  = adv_uuids;
	APP_OK(ble_advdata_set(&advdata, &scanrsp));
}
