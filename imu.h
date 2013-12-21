// vi:sw=4:ts=4

#pragma once

enum imu_sensor_set {
    IMU_SENSORS_DISABLED = 0,
    IMU_SENSORS_ACCEL = 1,
    IMU_SENSORS_GYRO = 2,
};

uint32_t imu_init(enum SPI_Channel channel);

uint16_t imu_accel_reg_read(uint8_t *buf);
uint16_t imu_read_regs(uint8_t *buf);

void imu_set_sensors(enum imu_sensor_set sensors);
enum imu_sensor_set imu_get_sensors();

uint16_t imu_fifo_size();
uint16_t imu_fifo_read(uint16_t count, uint8_t *buf);
void imu_fifo_reset();

