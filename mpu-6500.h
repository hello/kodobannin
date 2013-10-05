#pragma once
// Based on MPU-6500 Register Map and Descriptions Revision 2.0

#define CHIP_ID 0x70

enum MPU_Registers {
	MPU_REG_ACC_X_HI = 0x3B,
	MPU_REG_ACC_X_LO = 0x3C,
	MPU_REG_ACC_Y_HI = 0x3D,
	MPU_REG_ACC_Y_LO = 0x3E,
	MPU_REG_ACC_Z_HI = 0x3F,
	MPU_REG_ACC_Z_LO = 0x40,
	MPU_REG_TMP_HI   = 0x41,
	MPU_REG_TMP_LO   = 0x42,
	MPU_REG_GYRO_X_HI = 0x43,
	MPU_REG_GYRO_X_LO = 0x44,
	MPU_REG_GYRO_Y_HI = 0x45,
	MPU_REG_GYRO_Y_LO = 0x46,
	MPU_REG_GYRO_Z_HI = 0x47,
	MPU_REG_GYRO_Z_LO = 0x48,
	MPU_REG_WOM_THR  = 0x1F,
	MPU_REG_SIG_RST  = 0x68,
	MPU_REG_USER_CTL = 0x6A,
	MPU_REG_WHO_AM_I = 0x75,
};

enum MPU_Reg_Bits {
	USR_CTL_FIFO_EN  = (1UL << 6),
	USR_CTL_I2C_EN   = (1UL << 5),
	USR_CTL_I2C_DIS  = (1UL << 4),
	USR_CTL_FIFO_RST = (1UL << 2),
	USR_CTL_SIG_RST  = (1UL << 0),
}
