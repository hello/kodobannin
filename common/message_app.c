#include <stddef.h>
#include "message_app.h"
#include "util.h"


static struct{
    MSG_Central_t central;
    MSG_Base_t base;
    bool initialized;
    app_sched_event_handler_t unknown_handler;
    MSG_Base_t * mods[MSG_CENTRAL_MODULE_NUM]; 
}self;
static name = "CENTRAL";

typedef struct{
    MSG_Address_t src;
    MSG_Address_t dst;
    MSG_Data_t * data;
}future_event;

static void
_future_event_handler(void* event_data, uint16_t event_size){
    future_event * evt = event_data;
    uint8_t dst_idx = (uint8_t)evt->dst.module;
    if(dst_idx < MSG_CENTRAL_MODULE_NUM && self.mods[dst_idx]){
        self.mods[dst_idx]->send(evt->src,evt->dst, evt->data);
    }else{
        if(self.unknown_handler){
            self.unknown_handler(evt, sizeof(*evt));
        }
    }
    if(evt->data){
        MSG_Base_ReleaseDataAtomic(evt->data);
    }
}
static MSG_Status
_dispatch (MSG_Address_t src, MSG_Address_t  dst, MSG_Data_t * data){
    if(data){
        MSG_Base_AcquireDataAtomic(data);
    }
    {
        uint32_t err;
        future_event tmp = {src, dst, data};
        err = app_sched_event_put(&tmp, sizeof(tmp), _future_event_handler);
        err = MSG_Base_FreeCount();
        PRINTS("OC: ");
        PRINT_HEX(&err, 4);
        PRINTS("\r\n");
        if(!err){
            return SUCCESS;
        }else{
            return FAIL;
        }
    }
}
static MSG_Status
_loadmod(MSG_Base_t * mod){
    if(mod){
        if(mod->type < MSG_CENTRAL_MODULE_NUM){
            if(mod->init() == SUCCESS){
                self.mods[mod->type] = mod;
                return SUCCESS;
            }else{
                return FAIL;
            }
        }else{
            return OOM;
        }
    }
    return FAIL;
}

static MSG_Status
_unloadmod(MSG_Base_t * mod){
    MSG_Status ret;
    if(mod){
        if(mod->type < MSG_CENTRAL_MODULE_NUM){
            ret = mod->destroy();
            self.mods[mod->type] = NULL;
            return ret;
        }else{
            return OOM;
        }
    }
    return FAIL;
}

MSG_Central_t * MSG_App_Central( app_sched_event_handler_t unknown_handler ){
    if ( !self.initialized ){
        self.unknown_handler = unknown_handler; 
        self.initialized = 1;
        self.central.loadmod = _loadmod;
        self.central.unloadmod = _unloadmod;
        self.central.dispatch = _dispatch;
    }
    
    return &self.central;
}

static MSG_Status
_init(void){
    return SUCCESS;
}

static MSG_Status
_destroy(void){
    return SUCCESS;
}

static MSG_Status
_flush(void){
    return SUCCESS;
}
static MSG_Status
_send(MSG_Address_t src,MSG_Address_t dst, MSG_Data_t * data){
    if(data){
        MSG_Base_AcquireDataAtomic(data);
        MSG_AppCommand_t * tmp = data->buf;
        switch(tmp->cmd){
            default:
            case APP_PING:
                break;
            case APP_LSMOD:
                PRINTS("Loaded Mods\r\n");
                for(int i = 0; i < MSG_CENTRAL_MODULE_NUM; i++){
                    if(self.mods[i]){
                        PRINTS(self.mods[i]->typestr);
                        PRINTS("\r\n");
                    }
                }
                break;
        }
        MSG_Base_ReleaseDataAtomic(data);
    }
    return SUCCESS;
}
MSG_Base_t * MSG_App_Base(MSG_Central_t * parent){
    self.base.init = _init;
    self.base.destroy = _destroy;
    self.base.flush = _flush;
    self.base.send = _send;
    self.base.typestr = name;
    self.base.type = CENTRAL;
    return &self.base;
}
