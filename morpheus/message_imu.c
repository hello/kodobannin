#include "message_imu.h"
#include "app.h"
#include "platform.h"
#include <spi.h>
#include <util.h>
static const MSG_Central_t * parent;
static MSG_Base_t base;
static char * name = "IMU";

static MSG_Status _destroy(void){
    return SUCCESS;
}
static MSG_Status _flush(void){
    return SUCCESS;
}
static MSG_Status _init(void){
    return SUCCESS;
}
static MSG_Status _send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    return SUCCESS;
}
MSG_Base_t * MSG_IMU_Init(const MSG_Central_t * central){
	parent = central;
	base.init = _init;
	base.destroy = _destroy;
	base.flush = _flush;
	base.send = _send;
	base.type = IMU;
	base.typestr = name;
	return &base;
}
