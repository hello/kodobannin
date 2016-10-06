#include "message_imu.h"
#include "app.h"
#include "platform.h"
#include <spi.h>
#include <util.h>
#include "imu.h"
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
#ifdef PLATFORM_HAS_ACCEL_SPI
    if( 0 == imu_init_simple(SPI_Channel_0, SPI_Mode3, ACCEL_MISO, ACCEL_MOSI, ACCEL_SCLK, ACCEL_nCS)
        &&  0 == imu_tap_enable(0)
        ){
        return SUCCESS;
    }else{
        return FAIL;
    }
#else
    return FAIL;
#endif
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
