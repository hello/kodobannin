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
}

MSG_Base_t * MSG_Uart_Init(const app_uart_comm_params_t * params, MSG_Base_t * central){
    uint32_t err;
    return &self.base;
}
