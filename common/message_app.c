#include "message_app.h"
#include "util.h"


static struct{
    MSG_Base_t base;
    bool initialized;
    app_sched_event_handler_t notifier;
}self;

static void
_future_event(void* event_data, uint16_t event_size){
    MSG_Data_t * evt = *((MSG_Data_t**)event_data);
    if(self.notifier){
        self.notifier(evt, sizeof(*evt));
    }
    if(event_data){
        MSG_Base_ReleaseDataAtomic(evt);
    }
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
_send(MSG_Data_t * data){
    if(data){
        MSG_Base_AcquireDataAtomic(data);
    }
    app_sched_event_put(&data, sizeof(MSG_Data_t*), _future_event);
    return SUCCESS;
}

MSG_Base_t * MSG_App_Init( app_sched_event_handler_t handler ){
    if ( !self.initialized ){
        self.notifier = handler; 
        self.initialized = 1;
        self.base.init = _init;
        self.base.destroy = _destroy;
        self.base.flush = _flush;
        self.base.send = _send;
    }
    
    return &self.base;
}
