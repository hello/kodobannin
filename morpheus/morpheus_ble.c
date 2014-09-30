/* BLE transmission layer for Morpheus */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <app_timer.h>
#include <ble_gatts.h>
#include <ble_srv_common.h>
#include <ble_advdata.h>


#include "platform.h"
#include "app.h"
#include "ble_bondmngr_cfg.h"

#include "hlo_ble_time.h"
#include "util.h"
#include "message_app.h"
#include "message_sspi.h"
#include "message_uart.h"
#include "message_ant.h"
#include "message_ble.h"

#include "nrf.h"
#include "morpheus_ble.h"
#include "message_ble.h"

#include "ant_devices.h"
#include "ant_bondmgr.h"
#include "ant_user.h"

// To generate the protobuf download nanopb
// Generate C code: ~/nanopb-0.2.8-macosx-x86/generator-bin/protoc --nanopb_out=. morpheus/morpheus_ble.proto
// Generate Java code: protoc --java_out=. morpheus/morpheus_ble.proto



static MSG_Central_t * central; 
static MSG_Data_t* _protobuf_buffer;
static uint8_t _end_seq;
static uint8_t _seq_expected;
static uint16_t _protobuf_len;
static uint8_t _last_straw[10];

static void _unhandled_msg_event(void* event_data, uint16_t event_size){
	PRINTS("Unknown Event");
	
}

void create_bond(const ANT_BondedDevice_t * id){
	PRINTS("ID FOUND = ");
	PRINT_HEX(&id->full_uid,2);
	MSG_SEND_CMD(central, ANT, MSG_ANTCommand_t, ANT_ADD_DEVICE, &id->id, sizeof(id->id));
}




static bool _is_valid_protobuf(const struct hlo_ble_packet* header_packet)
{
	if(header_packet->sequence_number != 0)
	{
		PRINTS("Not the first packet\r\n");
		APP_OK(0);
	}

	if(18 + (header_packet->header.packet_count - 1) * 19 <= PROTOBUF_MAX_LEN)
	{
		return true;
	}

	return false;
}

static bool _decode_string_field(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    /* We could read block-by-block to avoid the large buffer... */
    if (stream->bytes_left > PROTOBUF_MAX_LEN - 1)
    {
        return false;
    }
   	
   	char str[PROTOBUF_MAX_LEN] = {0};
    if (!pb_read(stream, str, stream->bytes_left))
    {
        return false;
    }

	MSG_Data_t* string_page = MSG_Base_AllocateStringAtomic(str);
	*arg = string_page;

    return true;
}


static void _on_packet_arrival(void* event_data, uint16_t event_size)
{
	// The data_page is allocated BEFORE scheduling and NOT released by the schedule call.
	// So DONOT acquire data_page again here, or there will be memory leak.
	MSG_Data_t* data_page = *(MSG_Data_t**)event_data;

	PRINTS("Data len after scheduled: ");
	PRINT_HEX(&data_page->len, sizeof(data_page->len));
	PRINTS("\r\n");

/*
	PRINTS("Data after scheduled: ");
	PRINT_HEX(data_page->buf, data_page->len);
	PRINTS("\r\n");
*/

	MorpheusCommand command;
	memset(&command, 0, sizeof(command));

	
	command.accountId.funcs.decode = _decode_string_field;
	command.accountId.arg = NULL;

	command.deviceId.funcs.decode = _decode_string_field;
	command.deviceId.arg = NULL;

	pb_istream_t stream = pb_istream_from_buffer(data_page->buf, data_page->len);
    bool status = pb_decode(&stream, MorpheusCommand_fields, &command);
	

    if (!status)
    {
        PRINTS("Decoding protobuf failed, error: ");
        PRINTS(PB_GET_ERROR(&stream));
        PRINTS("\r\n");
    }else{

		message_ble_on_protobuf_command(data_page, &command);

		if(command.accountId.arg)
		{
			MSG_Base_ReleaseDataAtomic(command.accountId.arg);
		}

		if(command.deviceId.arg)
		{
			MSG_Base_ReleaseDataAtomic(command.deviceId.arg);
		}
	}

	// Don't forget to release the data_page.
	MSG_Base_ReleaseDataAtomic(data_page);
	
}


void morpheus_ble_write_handler(ble_gatts_evt_write_t* event)
{
	// This is the transmission layer that assemble the fucking protobuf.
	PRINTS("Protobuf received\r\n");

	struct hlo_ble_packet* ble_packet = (struct hlo_ble_packet*)event->data;
	if(ble_packet->sequence_number != _seq_expected){
		PRINTS("Unexpected sequence number:");
		PRINT_HEX(&ble_packet->sequence_number, sizeof(ble_packet->sequence_number));
		PRINTS(", expected: ");
		PRINT_HEX(&_seq_expected, sizeof(_seq_expected));
		PRINTS("\r\n");

		if(_protobuf_buffer){
			MSG_Base_ReleaseDataAtomic(_protobuf_buffer);
			_protobuf_buffer = NULL;
		}

		_seq_expected = 0;  // Reset the transmission and abort.

		return;
	}else{
		PRINTS("expected seq# ");
		PRINT_HEX(&_seq_expected, sizeof(_seq_expected));
		PRINTS("\r\n");

		PRINTS("rcv seq# ");
		PRINT_HEX(&ble_packet->sequence_number, sizeof(ble_packet->sequence_number));
		PRINTS("\r\n");
	}

	if(ble_packet->sequence_number == 0)
	{
		if(!_is_valid_protobuf(ble_packet))
		{
			PRINTS("Protobuf toooooo large!\r\n");
			_seq_expected = 0;
			return;
		}

		if(_protobuf_buffer)
		{
			// Possible race condition here, need to make sure no concurrency operation is allowed
			// from the phone.
			MSG_Base_ReleaseDataAtomic(_protobuf_buffer);
			_protobuf_buffer = NULL;
    	}

    	_protobuf_buffer = MSG_Base_AllocateDataAtomic(PROTOBUF_MAX_LEN);
    	if(!_protobuf_buffer){
    		PRINTS("Not enought memory\r\n");
    		return;
    	}
    	
		_protobuf_len = event->len - 2;   // seq# + total#, 2 bytes

		PRINTS("Payload length: ");
		PRINT_HEX(&_protobuf_len, sizeof(_protobuf_len));
		PRINTS("\r\n");

		memcpy(_protobuf_buffer->buf, ble_packet->header.data, _protobuf_len);  // seq# + total#, 2 bytes
		_end_seq = ble_packet->header.packet_count - 1;
	}else{
		size_t payload_len = event->len - 1;
		PRINTS("Payload length: ");
		PRINT_HEX(&payload_len, sizeof(payload_len));
		PRINTS("\r\n");

		memcpy(&_protobuf_buffer->buf[_protobuf_len], ble_packet->body.data, payload_len);

		// update the offset
		_protobuf_len += payload_len;
	}

	if(ble_packet->sequence_number == _end_seq)
	{
		MSG_Data_t* data_page = MSG_Base_AllocateDataAtomic(_protobuf_len);
		if(!data_page){
			PRINTS("Not enought memory\r\n");
		}else{
			memcpy(data_page->buf, _protobuf_buffer->buf, _protobuf_len);
		}

		MSG_Base_ReleaseDataAtomic(_protobuf_buffer);
		_protobuf_buffer = NULL;
		_seq_expected = 0;


		uint32_t err_code = app_sched_event_put(&data_page, sizeof(data_page), _on_packet_arrival);
		if(NRF_SUCCESS != err_code)
		{
			PRINTS("Scheduler error, transmission abort.\r\n");
			MSG_Base_ReleaseDataAtomic(data_page);
		}
	}else{
		_seq_expected = ble_packet->sequence_number + 1;
		PRINTS("expected seq set to: ");
		PRINT_HEX(&_seq_expected, sizeof(_seq_expected));
		PRINTS("\r\n");
	}

}


void morpheus_ble_on_notify_completed(void* data, void* data_page)
{
	MSG_Base_ReleaseDataAtomic((MSG_Data_t*)data_page);
}

void morpheus_ble_on_notify_failed(void* data_page)
{
	MSG_Base_ReleaseDataAtomic((MSG_Data_t*)data_page);
}

bool morpheus_ble_reply_protobuf(const MorpheusCommand* morpheus_command){
	MSG_Data_t* heap_page = MSG_Base_AllocateDataAtomic(PROTOBUF_MAX_LEN);
	memset(heap_page->buf, 0, heap_page->len);

	pb_ostream_t stream = pb_ostream_from_buffer(heap_page->buf, heap_page->len);
	bool status = pb_encode(&stream, MorpheusCommand_fields, morpheus_command);
    
    if(status)
    {
    	size_t protobuf_len = stream.bytes_written;
		hlo_ble_notify(0xB00B, heap_page->buf, protobuf_len, 
			&(struct hlo_ble_operation_callbacks){morpheus_ble_on_notify_completed, morpheus_ble_on_notify_failed, heap_page});
	}else{
		PRINTS("encode protobuf failed: ");
		PRINTS(PB_GET_ERROR(&stream));
		PRINTS("\r\n");
		MSG_Base_ReleaseDataAtomic(heap_page);
	}

	return status;
}

bool morpheus_ble_reply_protobuf_error(uint32_t error_type)
{
    MorpheusCommand morpheus_command;
    memset(&morpheus_command, 0, sizeof(morpheus_command));
    morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_ERROR;
    morpheus_command.version = PROTOBUF_VERSION;

    morpheus_command.has_error = true;
    morpheus_command.error = error_type;

    // We shall NOT use morpheus_ble_reply_protobuf to reply error
    // because when out of memory happens, the heap will not be available
    memset(_last_straw, 0, sizeof(_last_straw));

    pb_ostream_t stream = pb_ostream_from_buffer(_last_straw, sizeof(_last_straw));
	bool status = pb_encode(&stream, MorpheusCommand_fields, &morpheus_command);
    
    if(status)
    {
    	size_t protobuf_len = stream.bytes_written;
		hlo_ble_notify(0xB00B, _last_straw, protobuf_len, NULL);
	}else{
		PRINTS("encode protobuf failed: ");
		PRINTS(PB_GET_ERROR(&stream));
		PRINTS("\r\n");
	}

	return status;
}

void morpheus_ble_transmission_layer_init()
{
	_seq_expected = 0;
	_end_seq = 0;
	_protobuf_len = 0;
	_protobuf_buffer = NULL;
}

void morpheus_ble_transmission_layer_reset()
{
	// 1. Reset the application layer
	message_ble_reset();

	// 2. reset transmission layer.
	_seq_expected = 0;
	_end_seq = 0;
	_protobuf_len = 0;
	if(!_protobuf_buffer)
	{
		MSG_Base_ReleaseDataAtomic(_protobuf_buffer);
	}
	_protobuf_buffer = NULL;
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
    		APP_UART_FLOW_CONTROL_ENABLED,
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
		central->loadmod(MSG_BLE_Base(central));
        central->loadmod(MSG_Cli_Base(central, Cli_User_Init(NULL)));
#ifdef ANT_ENABLE
        central->loadmod(MSG_ANT_Base(central, ANT_UserInit(central)));
        {
            hlo_ant_role role = HLO_ANT_ROLE_CENTRAL;
            MSG_SEND_CMD(central, ANT, MSG_ANTCommand_t, ANT_SET_ROLE, &role,sizeof(role));
        }
        {
            ANT_BondMgrInit();
            ANT_BondMgrForEach(create_bond);
        }
#endif
        MSG_SEND_CMD(central, CENTRAL, MSG_AppCommand_t, APP_LSMOD,NULL,0);
    }else{
        PRINTS("FAIL");
    }
}
