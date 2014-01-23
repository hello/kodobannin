// vi:sw=4:ts=4

#pragma once

enum imu_sensor_set {
    IMU_SENSORS_DISABLED = 0,
    IMU_SENSORS_ACCEL = 1,
    IMU_SENSORS_GYRO = 2,
};

enum imu_wake_on_motion_hz {
	IMU_WOM_HZ_0_25 = 0,
	IMU_WOM_HZ_0_49 = 1,
    IMU_WOM_HZ_0_98 = 2,
    IMU_WOM_HZ_1_95 = 3,
    IMU_WOM_HZ_3_91 = 4,
    IMU_WOM_HZ_7_81 = 5,
    IMU_WOM_HZ_15_63 = 6,
    IMU_WOM_HZ_31_25 = 7,
    IMU_WOM_HZ_62_50 = 8,
    IMU_WOM_HZ_125 = 9,
    IMU_WOM_HZ_250 = 10,
    IMU_WOM_HZ_500 = 11,
};

void imu_init(enum SPI_Channel channel);

uint16_t imu_accel_reg_read(uint8_t *buf);
uint16_t imu_read_regs(uint8_t *buf);
uint8_t imu_get_sample_rate();
void imu_set_sample_rate(uint8_t hz);

void imu_set_sensors(enum imu_sensor_set sensors);
enum imu_sensor_set imu_get_sensors();

uint16_t imu_fifo_bytes_available();
uint16_t imu_fifo_read(uint16_t count, uint8_t *buf);
void imu_fifo_clear();

void imu_wakeup();

void imu_wake_on_motion(uint16_t microgravities, enum imu_wake_on_motion_hz wom_hz);
bool imu_did_wake_on_motion();

uint32_t imu_timer_ticks_from_sample_rate(uint8_t hz);

#define IMU_FIFO_CAPACITY 4096 // Must be 512, 1024, 2048 or 4096
