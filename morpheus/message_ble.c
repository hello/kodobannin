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
		// protobuf, dump the thing straight back?
		hlo_ble_notify(0xB00B, _buffer, data->len, NULL);
    }
}

static MSG_Status _init(){
    return SUCCESS;
}

