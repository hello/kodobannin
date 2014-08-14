// vi:noet:sw=4 ts=4

#pragma once

#include "spi_nor.h"

#include "imu_data.h"
#include "message_base.h"

struct sensor_data_header;

typedef void(*imu_data_ready_callback_t)(uint16_t fifo_bytes_available);
typedef void(*imu_wom_callback_t)(const int16_t* raw_data_point, size_t len);

struct imu_settings {
	uint16_t wom_threshold; // in microgravities
    enum imu_hz low_power_mode_sampling_rate;
    enum imu_sensor_set active_sensors;
	enum imu_hz normal_mode_sampling_rate;
	enum imu_accel_range accel_range;
    enum imu_gyro_range gyro_range;
    unsigned ticks_to_fill_fifo;
	unsigned ticks_to_fifo_watermark;
	bool active;
    imu_data_ready_callback_t data_ready_callback;
    imu_wom_callback_t wom_callback;
};

typedef struct{
    enum{
        IMU_PING=0,
		IMU_READ_XYZ
    }cmd;
    union{
		struct imu_accel16_sample out_accel;
    }param;
}MSG_IMUCommand_t;

/* See README_IMU.md for an introduction to the IMU, and vocabulary
   that you may need to understand the rest of this. */

MSG_Base_t * MSG_IMU_Init(const MSG_Central_t * central);
void imu_get_settings(struct imu_settings* settings);

bool imu_is_active();

void imu_set_data_ready_callback(imu_data_ready_callback_t callback);
void imu_set_wom_callback(imu_wom_callback_t callback);
