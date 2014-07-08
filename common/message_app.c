#include <stddef.h>
#include "message_app.h"
#include "util.h"


static struct{
    MSG_Central_t base;
    bool initialized;
    app_sched_event_handler_t unknown_handler;
    MSG_Base_t * mods[MSG_CENTRAL_MODULE_NUM]; 
}self;

typedef struct{
    MSG_ModuleType src;
    MSG_ModuleType dst;
    MSG_Data_t * data;
}future_event;

static void
_future_event_handler(void* event_data, uint16_t event_size){
    future_event * evt = event_data;
    if(evt->dst < MSG_CENTRAL_MODULE_NUM && self.mods[evt->dst]){
        PRINTS("FOUND");
        self.mods[evt->dst]->send(evt->src, evt->data);
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
_send (MSG_ModuleType src, MSG_ModuleType dst, MSG_Data_t * data){
    if(data){
        MSG_Base_AcquireDataAtomic(data);
    }
    {
        future_event tmp = {src, dst, data};
        app_sched_event_put(&tmp, sizeof(tmp), _future_event_handler);
        return SUCCESS;
    }
}
static MSG_Status
_loadmod(MSG_Base_t * mod){
    if(mod){
        if(mod->type < MSG_CENTRAL_MODULE_NUM){
            self.mods[mod->type] = mod;
            return SUCCESS;
        }else{
            return OOM;
        }
    }
    return FAIL;
}

static MSG_Status
_unloadmod(MSG_Base_t * mod){
    if(mod){
        if(mod->type < MSG_CENTRAL_MODULE_NUM){
            self.mods[mod->type] = NULL;
            return SUCCESS;
        }else{
            return OOM;
        }
    }
    return FAIL;
}

MSG_Central_t * MSG_App_Init( app_sched_event_handler_t unknown_handler ){
    if ( !self.initialized ){
        self.unknown_handler = unknown_handler; 
        self.initialized = 1;
        self.base.loadmod = _loadmod;
        self.base.unloadmod = _unloadmod;
        self.base.send = _send;
    }
    
    return &self.base;
}
