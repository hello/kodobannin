// vi:noet:sw=4 ts=4
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <app_timer.h>
#include <ble_gatts.h>
#include <ble_srv_common.h>
#include <ble_advdata.h>

#include "app.h"
#include "platform.h"

#include "hlo_ble_time.h"
#include "util.h"
#include "message_app.h"
#include "message_sspi.h"
#include "message_uart.h"
#include "message_ant.h"

#include "nrf.h"
#include "morpheus_ble.h"

#ifdef DEBUG_SERIAL
#define PB_NO_ERRMSG
#endif

#include "pb_decode.h"
#include "pb_encode.h"
#include "morpheus_ble.pb.h"

extern uint8_t hello_type;

static uint16_t _morpheus_service_handle;
static MSG_Central_t * central; 
static uint8_t _protobuf_buffer[256];
static uint8_t _end_seq;
static uint8_t _seq_expected;
static uint16_t _protobuf_len;

static void _morpheus_switch_mode(void*, uint16_t);
static void _led_pairing_mode(void);

static void _unhandled_msg_event(void* event_data, uint16_t event_size){
	PRINTS("Unknown Event");
	
}


static void
_data_send_finished()
{
	PRINTS("DONE!");
}

static bool _is_valid_protobuf(const struct hlo_ble_packet* header_packet)
{
	if(header_packet->sequence_number != 0)
	{
		PRINTS("Not the first packet\r\n");
		APP_OK(0);
	}

	if(18 + (header_packet->header.packet_count - 1) * 19 <= sizeof(_protobuf_buffer))
	{
		return true;
	}

	return false;
}

static void _on_packet_arrival(void* event_data, uint16_t event_size){
	struct hlo_ble_packet* ble_packet = (struct hlo_ble_packet*)event_data;
	if(ble_packet->sequence_number == 0)
	{
		_protobuf_len = event_size - 2;   // seq# + total#, 2 bytes
		memcpy(_protobuf_buffer, ble_packet->header.data, event_size - 2);  // seq# + total#, 2 bytes
		_end_seq = ble_packet->sequence_number + ble_packet->header.packet_count - 1;
	}else{
		memcpy(&_protobuf_buffer[_protobuf_len], ble_packet->body.data, event_size - 1);

		// update the offset
		_protobuf_len += event_size - 1;
		if(ble_packet->sequence_number == _end_seq)
		{
			MorpheusCommand command;
			pb_istream_t stream = pb_istream_from_buffer(_protobuf_buffer, _protobuf_len);
	        bool status = pb_decode(&stream, MorpheusCommand_fields, &command);
	        
	        if (!status)
	        {
	            PRINTS("Decoding protobuf failed, error: ");
	            PRINTS(PB_GET_ERROR(&stream));
	            PRINTS("\r\n");
	            return;
	        }

			switch(command.type)
			{
				case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:
					
					hble_set_advertising_mode(true);
					hlo_ble_notify(0xB00B, _protobuf_buffer, _protobuf_len, NULL);
					_led_pairing_mode();
					break;
				case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE:
					
					hble_set_advertising_mode(false);
					hlo_ble_notify(0xB00B, _protobuf_buffer, _protobuf_len, NULL);
					_led_pairing_mode();
					break;
				case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
					break;
				default:
					break;
			}
			

		}
	}
	
}


static void _protobuf_command_write_handler(ble_gatts_evt_write_t* event)
{
	struct hlo_ble_packet* ble_packet = (struct hlo_ble_packet*)event->data;
	if(ble_packet->sequence_number != _seq_expected){
		return;
	}

	if(ble_packet->sequence_number == 0)
	{
		if(!_is_valid_protobuf(ble_packet))
		{
			PRINTS("Protobuf toooooo large!\r\n");
			return;
		}
	}

	
	_seq_expected = ble_packet->sequence_number + 1;

	if(_end_seq < _seq_expected)
	{
		_seq_expected = 0;
	}
	app_sched_event_put(event->data, event->len, _on_packet_arrival);
}

static void _command_write_handler(ble_gatts_evt_write_t* event)
{
	struct morpheus_command* command = (struct morpheus_command*)event->data;
	bool pairing_mode = false;
	switch(command->command)
	{
		case MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:
			pairing_mode = true;
			app_sched_event_put(&pairing_mode, sizeof(pairing_mode), _morpheus_switch_mode);
			break;
		case MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE:
			pairing_mode = false;
			app_sched_event_put(&pairing_mode, sizeof(pairing_mode), _morpheus_switch_mode);
			break;
		case MORPHEUS_COMMAND_START_WIFISCAN:
			
			break;
		case MORPHEUS_COMMAND_STOP_WIFISCAN:
			
			break;
		default:
			break;
	}
	

}

static void _led_pairing_mode(void)
{
	// TODO: Notify the led
}

static void _morpheus_switch_mode(void* event_data, uint16_t event_size)
{
	bool* mode = (bool*)event_data;
	hble_set_advertising_mode(*mode);
#ifdef PROTO_REPLY
	uint8_t reply_proto_buffer[20];
	MorpheusCommand command;
	command.type = (*mode) ? MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE :
		MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE;
	command.version = 0;
	pb_ostream_t stream = pb_ostream_from_buffer(reply_proto_buffer, sizeof(reply_proto_buffer));
	bool status = pb_encode(&stream, MorpheusCommand_fields, &command);
    size_t protobuf_len = stream.bytes_written;
    if(status)
    {
		hlo_ble_notify(0xD00D, reply_proto_buffer, protobuf_len, NULL);
		_led_pairing_mode();
	}else{
		PRINTS("encode protobuf failed: ");
		PRINTS(PB_GET_ERROR(&stream));
		PRINTS("\r\n");
	}

#else
	PRINTS("reply with raw memory layout\r\n");
	hlo_ble_notify(0xD00D, &((struct morpheus_command*)event_data)->command, event_size, NULL);
	_led_pairing_mode();
#endif
}

void morpheus_ble_transmission_layer_init()
{
	_seq_expected = 0;
	_end_seq = 0;
	_protobuf_len = 0;
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

        hlo_ble_char_write_request_add(0xBEEB, &_protobuf_command_write_handler, sizeof(struct hlo_ble_packet));
        hlo_ble_char_notify_add(0xB00B);

        hlo_ble_char_notify_add(0xFEE1);
        
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
		MSG_SEND(central, CENTRAL, APP_LSMOD,NULL,0);

#ifdef ANT_ENABLE
		MSG_SEND(central, ANT, ANT_SET_ROLE, 0,1);
#endif
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
