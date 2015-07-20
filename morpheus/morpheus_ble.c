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
#include "ant_user.h"
#include "cli_user.h"
#include "sspi_user.h"

// To generate the protobuf download nanopb
// Generate C code: ~/nanopb-0.2.8-macosx-x86/generator-bin/protoc --nanopb_out=. morpheus/morpheus_ble.proto
// ~/nanopb-0.3.1-macosx-x86/generator-bin/protoc --nanopb_out=. morpheus/morpheus_ble.proto
// Generate Java code: protoc --java_out=. morpheus/morpheus_ble.proto



static MSG_Central_t * central; 

static void _unhandled_msg_event(void* event_data, uint16_t event_size){
	PRINTS("Unknown Event");
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
		central->loadmod(MSG_SSPI_Base(&spi_params, spi_handler, central));
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
#endif
		central->dispatch( ADDR(CENTRAL, 0), ADDR(CENTRAL,MSG_APP_LSMOD), NULL);
    }else{
        PRINTS("FAIL");
    }

}
