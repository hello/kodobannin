#include <stdlib.h>
#include "message_ble.h"

#include "app.h"
#include "platform.h"
#include "hble.h"
#include "morpheus_ble.h"

#ifdef MSG_BASE_USE_BIG_POOL
static uint8_t _buffer[MSG_BASE_DATA_BUFFER_SIZE_BIG];
#else
static uint8_t _buffer[MSG_BASE_DATA_BUFFER_SIZE];
#endif

static MSG_Status _destroy(void);
static MSG_Status _flush(void);
static MSG_Status _init();
static MSG_Status _on_data_arrival(MSG_Address_t, MSG_Address_t, MSG_Data_t*);

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
} self;

MSG_Base_t* MSG_BLE_Base(MSG_Central_t* parent){
    self.parent = parent;
    {
        self.base.init =  _init;
        self.base.flush = _flush;
        self.base.send = _on_data_arrival;
        self.base.destroy = _destroy;
        self.base.type = BLE;
        self.base.typestr = "BLE";
    }
    return &self.base;

}

static MSG_Status _destroy(void){
    return SUCCESS;
}

static MSG_Status _flush(void){
    return SUCCESS;
}

static MSG_Status _on_data_arrival(MSG_Address_t src, MSG_Address_t dst,  MSG_Data_t* data){
    if(dst.submodule == 0){
    	// If we send things more than 1 packet, we might get fucked here.
    	memset(_buffer, 0, sizeof(_buffer));
    	memcpy(_buffer, data, data->len);
#ifdef PROTO_REPLY
		// protobuf, dump the thing straight back?
		hlo_ble_notify(0xB00B, _buffer, data->len, NULL);
#else
		struct morpheus_command* command = (struct morpheus_command*)_buffer;
        switch(command->command){
            
            case MORPHEUS_COMMAND_SET_LED:
            	// do nothing.
                PRINTS("SET LED ACK\r\n");
                break;
            case MORPHEUS_COMMAND_GET_DEVICE_ID:
            	PRINTS("Device ID: ");
            	PRINT_HEX(command->device_id, sizeof(command->device_id));
            	PRINTS("\r\n");
        		hlo_ble_notify(0xD00D, &command, data->len, NULL);
            	break;
        	case MORPHEUS_COMMAND_START_WIFISCAN:
        		PRINTS("WIFI EP received\r\n");
        		hlo_ble_notify(0xD00D, &command, data->len, NULL);
        		break;
        	case MORPHEUS_COMMAND_SET_WIFI_ENDPOINT:
        	case MORPHEUS_COMMAND_GET_WIFI_ENDPOINT:
        		hlo_ble_notify(0xD00D, &command, data->len, NULL);
        		PRINTS("wifi password set.\r\n");
        		break;
        }
#endif
    }
}

static MSG_Status _init(){
    return SUCCESS;
}

