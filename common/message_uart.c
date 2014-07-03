#include "message_uart.h"
#include "util.h"

static struct{
    MSG_Base_t base;
    bool initialized;
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
    return SUCCESS;
}
static void
_uart_event_handler(app_uart_evt_t * evt){
    PRINTS("Event");
    app_uart_put('1');
}

MSG_Base_t * MSG_Uart_Init(const app_uart_comm_params_t * params, MSG_Base_t * central){
    uint32_t err;
    if ( !self.initialized ){
        APP_UART_INIT(params,  _uart_event_handler, APP_IRQ_PRIORITY_LOW,err);
        app_uart_put('1');
        app_uart_put('1');
        app_uart_put('1');
        app_uart_put('1');
        app_uart_put('1');
        app_uart_put('1');
        if(!err){
            PRINTS("UART ERROR");
        }else{
            self.initialized = 1;
            self.base.init = _init;
            self.base.destroy = _destroy;
            self.base.flush = _flush;
            self.base.queue = _queue;
        }
    }
    return &self.base;
}
