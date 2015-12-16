#include "message_prox.h"
#include "platform.h"

static char * name = "PROX";
static const MSG_Central_t * parent;
static MSG_Base_t base;
static bool initialized = false;

MSG_Status _init_prox(void){
#ifdef PLATFORM_HAS_PROX
    return SUCCESS;
#else
    return SUCCESS;
#endif
}
static MSG_Status _init(void){
    return _init_prox();
}
static MSG_Status _destroy(void){
    return SUCCESS;
}
static MSG_Status _flush(void){
    return SUCCESS;
}
static MSG_Status _on_message(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    PRINTS("PROX CMD\r\n");
}
uint16_t MSG_Prox_Read(void){
    return 0xBEEF;
}
MSG_Base_t * MSG_Prox_Init(const MSG_Central_t * central){
	parent = central;
	base.init = _init;
	base.destroy = _destroy;
	base.flush = _flush;
	base.send = _on_message;
	base.type = PROX;
	base.typestr = name;
    return &base;
}
