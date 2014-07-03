#include "message_app.h"
#include "util.h"

static struct{
    MSG_Base_t base;
    bool initialized;
    app_sched_event_handler_t notifier;
}self;

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
_queue(MSG_Data_t * data){
    uint32_t ret = app_sched_event_put(&ret, sizeof(ret), self.notifier);
    PRINT_HEX(&ret, sizeof(ret));

    return SUCCESS;
}

MSG_Base_t * MSG_App_Init( app_sched_event_handler_t handler ){
    if ( !self.initialized ){
        self.notifier = handler; 
        self.initialized = 1;
        self.base.init = _init;
        self.base.destroy = _destroy;
        self.base.flush = _flush;
        self.base.queue = _queue;
    }
    
    return &self.base;
}
