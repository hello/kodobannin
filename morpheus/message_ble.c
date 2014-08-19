#include <stdlib.h>
#include "message_ble.h"

#include "app.h"
#include "platform.h"
#include "hble.h"
#include "morpheus_ble.h"

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
} self;



static MSG_Status _destroy(void){
    return SUCCESS;
}

static MSG_Status _flush(void){
    return SUCCESS;
}

static void _on_hlo_notify_completed(const void* data_sent, void* tag){
	MSG_Base_ReleaseDataAtomic((MSG_Data_t*)tag);
}

static void _on_hlo_notify_failed(void* tag){
    MSG_Base_ReleaseDataAtomic((MSG_Data_t*)tag);
}

static MSG_Status _on_data_arrival(MSG_Address_t src, MSG_Address_t dst,  MSG_Data_t* data){
    if(dst.submodule == 0){
    	// If we send things more than 1 packet, we might get fucked here.
    	MSG_Base_AcquireDataAtomic(data);
		// protobuf, dump the thing straight back?
		hlo_ble_notify(0xB00B, data->buf, data->len, 
            &(struct hlo_ble_operation_callbacks){_on_hlo_notify_completed, _on_hlo_notify_failed, data});
    }
}

MSG_Status route_data_to_cc3200(const MSG_Data_t* data){
    
    if(data){
        self.parent->dispatch((MSG_Address_t){BLE, 0},(MSG_Address_t){SSPI, 1}, data);
        
        return SUCCESS;
    }else{
        return FAIL;
    }
}

static MSG_Status _init(){
    return SUCCESS;
}

MSG_Base_t* MSG_BLE_Base(MSG_Central_t* parent){
    self.parent = parent;
    
    self.base.init =  _init;
    self.base.flush = _flush;
    self.base.send = _on_data_arrival;
    self.base.destroy = _destroy;
    self.base.type = BLE;
    self.base.typestr = "BLE";
    
    return &self.base;

}

