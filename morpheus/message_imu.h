#include "spi_nor.h"
#include "message_base.h"

typedef enum{
	IMU_PING = 0,
	IMU_READ_XYZ,
	IMU_SELF_TEST,
	IMU_FORCE_SHAKE,
}MSG_IMUAddress;

MSG_Base_t * MSG_IMU_Init(const MSG_Central_t * central);
