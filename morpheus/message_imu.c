#include "message_imu.h"
#include "app.h"
#include "platform.h"
#include <spi.h>
#include <util.h>
#include "imu.h"
#include <nrf_gpio.h>
#include <app_gpiote.h>
static const MSG_Central_t * parent;
static app_gpiote_user_id_t _gpiote_user;
static MSG_Base_t base;
static char * name = "IMU";

static MSG_Status _destroy(void){
    return SUCCESS;
}
static MSG_Status _flush(void){
    return SUCCESS;
}

static void _imu_gpiote_process(uint32_t event_pins_low_to_high, uint32_t event_pins_high_to_low)
{
    imu_clear_interrupt_status();
    PRINTS("Tap\r\n");
    MSG_Data_t * data = MSG_Base_AllocateStringAtomic("tap");
    if(data){
        parent->dispatch(  (MSG_Address_t){IMU, 0},
                (MSG_Address_t){SSPI,MSG_SSPI_TEXT},
                data);
        MSG_Base_ReleaseDataAtomic(data);
    }
}
static MSG_Status _init(void){
#ifdef PLATFORM_HAS_ACCEL_SPI
    if( 0 == imu_init_simple(SPI_Channel_0, SPI_Mode3, ACCEL_MISO, ACCEL_MOSI, ACCEL_SCLK, ACCEL_nCS)
        &&  0 == imu_tap_enable()
    ){
        APP_OK(app_gpiote_user_enable(_gpiote_user));
        imu_clear_interrupt_status();
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
    nrf_gpio_cfg_input(ACCEL_INT, NRF_GPIO_PIN_NOPULL);
	APP_OK(app_gpiote_user_register(&_gpiote_user, 0, 1 << ACCEL_INT, _imu_gpiote_process));
	APP_OK(app_gpiote_user_disable(_gpiote_user));
	return &base;
}
