// vi:sw=4:ts=4

#pragma once

#include <stdint.h>

enum imu_sensor_set {
	IMU_SENSORS_ACCEL = 0,
	IMU_SENSORS_ACCEL_GYRO = 1,
};

enum imu_hz {
	IMU_HZ_0_25 = 0,
	IMU_HZ_0_49 = 1,
	IMU_HZ_0_98 = 2,
	IMU_HZ_1_95 = 3,
	IMU_HZ_3_91 = 4,
	IMU_HZ_7_81 = 5,
	IMU_HZ_15_63 = 6,
	IMU_HZ_31_25 = 7,
	IMU_HZ_62_50 = 8,
	IMU_HZ_125 = 9,
	IMU_HZ_250 = 10,
	IMU_HZ_500 = 11,
};

enum imu_accel_range {
	IMU_ACCEL_RANGE_2G = 0,
	IMU_ACCEL_RANGE_4G = 1,
	IMU_ACCEL_RANGE_8G = 2,
	IMU_ACCEL_RANGE_16G = 3,
};

enum imu_gyro_range {
	IMU_GYRO_RANGE_250_DPS = 0,
	IMU_GYRO_RANGE_500_DPS = 1,
	IMU_GYRO_RANGE_1000_DPS = 2,
	IMU_GYRO_RANGE_2000_DPS = 3,
};

/// This is _unused_ right now; just here for safe-keeping.
enum imu_resolution {
	IMU_RESOLUTION_8_BITS = 0,
	IMU_RESOLUTION_10_BITS = 1,
	IMU_RESOLUTION_12_BITS = 2,
	IMU_RESOLUTION_16_BITS = 3,
};

struct imu_profile {
	enum imu_sensor_set sensors:1;
	enum imu_accel_range accel_range:2;
	enum imu_gyro_range gyro_range:2;
	enum imu_resolution resolution:2;
	enum imu_hz hz:4;
	enum imu_hz sleep_hz:4;
	uint8_t wom_threshold; //< in microgravities/4, e.g. 36mg = 8
} PACKED;  //< Should be 3 bytes in total.

// IMU accelerometer-only data

// Note: All int16_t's in this struct are _BIG-ENDIAN_.
struct imu_accel16_sample {
    int16_t x;
    int16_t y;
    int16_t z;
} PACKED;

// struct imu_accel_data {
//     struct imu_accel16_sample samples[];
// } PACKED;

// IMU accelerometer+gyroscope data

// Note: All int16_t's in this struct are _BIG-ENDIAN_.
struct imu_gyro16_sample {
    int16_t x;
    int16_t y;
    int16_t z;
} PACKED;

struct imu_accel_gyro16_sample {
    struct imu_accel16_sample accel;
    struct imu_gyro16_sample gyro;
} PACKED;

// struct imu_accel_gyro_data {
//     struct imu_accel_gyro16_sample samples[];
// } PACKED;
