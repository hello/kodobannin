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
#include "cli_user.h"
#include "proto_tools.h"

// To generate the protobuf download nanopb
// Generate C code: ~/nanopb-0.2.8-macosx-x86/generator-bin/protoc --nanopb_out=. morpheus/morpheus_ble.proto
// ~/nanopb-0.3.1-macosx-x86/generator-bin/protoc --nanopb_out=. morpheus/morpheus_ble.proto
// Generate Java code: protoc --java_out=. morpheus/morpheus_ble.proto



static MSG_Central_t * central; 
static MSG_Data_t* _protobuf_buffer;
static uint8_t _end_seq;
static uint8_t _seq_expected;
static uint16_t _protobuf_len;

static void _unhandled_msg_event(void* event_data, uint16_t event_size){
	PRINTS("Unknown Event");
	
}

void create_bond(const ANT_BondedDevice_t * id){
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
    /*
    PRINTS("data_page addr in _on_packet_arrival:");
    PRINT_HEX(&data_page, sizeof(data_page));
    PRINTS("\r\n");
    */

	MorpheusCommand command = {0};
    if(MorpheusCommand_from_message(&command, data_page)){
        // Becareful, we should either redefine another data_page here
        // or use *(MSG_Data_t**)event_data straight to make sure
        // the data_page pointer will not get optimized out.
        message_ble_on_protobuf_command(data_page, &command);
        free_MorpheusCommand(&command);
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
    		PRINTS(MSG_NO_MEMORY);
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
			PRINTS(MSG_NO_MEMORY);
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


bool morpheus_ble_reply_protobuf(MorpheusCommand* morpheus_command){
    MSG_Data_t * heap_page = message_from_MorpheusCommand(morpheus_command);
    if(heap_page){
        central->dispatch(ADDR(CENTRAL, 0), ADDR(BLE, MSG_BLE_DEFAULT_CONNECTION), heap_page);
        MSG_Base_ReleaseDataAtomic(heap_page);
        return true;
    }
    return false;
}

bool morpheus_ble_reply_protobuf_error(uint32_t error_type)
{
    MorpheusCommand morpheus_command;
    memset(&morpheus_command, 0, sizeof(morpheus_command));
    morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_ERROR;

    morpheus_command.has_error = true;
    morpheus_command.error = error_type;

    return morpheus_ble_reply_protobuf(&morpheus_command);
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
	if(_protobuf_buffer)
	{
		MSG_Base_ReleaseDataAtomic(_protobuf_buffer);
	}
	_protobuf_buffer = NULL;
}

void morpheus_load_modules(void){
    central = MSG_App_Central(_unhandled_msg_event );
    if(central){
    	central->loadmod(MSG_App_Base(central));
#ifdef PLATFORM_HAS_SERIAL_CROSS_CONNECT
		app_uart_comm_params_t uart_params = {
            CCU_RX_PIN,
            CCU_TX_PIN,
            CCU_CTS_PIN,
            CCU_RTS_PIN,
            APP_UART_FLOW_CONTROL_DISABLED,
            0,
            UART_BAUDRATE_BAUDRATE_Baud38400
		};

		central->loadmod(MSG_Uart_Base(&uart_params, central));
#else
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

#ifdef BLE_ENABLE
		central->loadmod(MSG_BLE_Base(central));
#endif 

        central->loadmod(MSG_Cli_Base(central, Cli_User_Init(central, NULL)));
#ifdef ANT_ENABLE
        central->loadmod(MSG_ANT_Base(central, ANT_UserInit(central)));
        {
            hlo_ant_role role = HLO_ANT_ROLE_CENTRAL;
 			MSG_Data_t * prole = MSG_Base_AllocateObjectAtomic(&role, sizeof(role));
			if(prole){
				central->dispatch(ADDR(CENTRAL,0), ADDR(ANT, MSG_ANT_SET_ROLE), prole);
				MSG_Base_ReleaseDataAtomic(prole);
			}
        }
        {
            ANT_BondMgrInit();
            ANT_BondMgrForEach(create_bond);
        }
#endif
		central->dispatch( ADDR(CENTRAL, 0), ADDR(CENTRAL,MSG_APP_LSMOD), NULL);
    }else{
        PRINTS("FAIL");
    }

}
