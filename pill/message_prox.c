#include "message_prox.h"
#include "platform.h"
#include "util.h"
#include "prox_i2c.h"
#include "twi_master.h"
#include "message_ant.h"
#include "app_timer.h"
#include "twi_master_config.h"

static char * name = "PROX";
static const MSG_Central_t * parent;
static MSG_Base_t base;
static app_timer_id_t timer_id_prox;

#define PROX_POLL_INTERVAL 15000 /*in ms*/

static MSG_Status _init(void){
    uint32_t ticks = 0;
    ticks = APP_TIMER_TICKS(PROX_POLL_INTERVAL,APP_TIMER_PRESCALER);
    PRINTF("PROX %u ms\r\n", PROX_POLL_INTERVAL);
    app_timer_start(timer_id_prox, ticks, NULL);
    return init_prox();
}
static MSG_Status _destroy(void){
    return SUCCESS;
}
static MSG_Status _flush(void){
    return SUCCESS;
}
static MSG_Status _on_message(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    PRINTS("PROX CMD\r\n");
    return SUCCESS;
}
static void _send_available_prox_ant(){
    pill_proxdata_t prox = {0};
    read_prox(&prox.cap[0], &prox.cap[1]);
    PRINTF("P1: %u, P4: %u \r\n", prox.cap[0], prox.cap[1]);
    MSG_Data_t * data = AllocateEncryptedAntPayload(ANT_PILL_PROX_ENCRYPTED, &prox, sizeof(prox));
    if(data){
        parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){ANT,1}, data);
        /*
         *parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){UART,MSG_UART_HEX}, data);
         */
        MSG_Base_ReleaseDataAtomic(data);
    }

}
static void _prox_timer_handler(void * ctx) {
    _send_available_prox_ant();
}
MSG_Base_t * MSG_Prox_Init(const MSG_Central_t * central){
	parent = central;
	base.init = _init;
	base.destroy = _destroy;
	base.flush = _flush;
	base.send = _on_message;
	base.type = PROX;
	base.typestr = name;

    twi_master_init();
    APP_OK(app_timer_create(&timer_id_prox,APP_TIMER_MODE_REPEATED, _prox_timer_handler));
    return &base;
}
