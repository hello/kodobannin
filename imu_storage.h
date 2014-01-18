// vi:sw=4:ts=4

#pragma once

struct imu_version_header {
    uint8_t version; //< version number. If this is 255, expect a secondary version number to follow, which should be added to this version number to get the real version. Use struct imu_data_header_N" to parse the version number.
} __attribute__((packed));

struct imu_data_header_0 {
	uint32_t timestamp;  //< number of seconds since Jan 1, 2014.
	bool gyro_xyz:1; //< If true, data has gyro XYZ values.
	bool accel_xyz:1; //< If true, data has accelerometer XYZ values.
	uint16_t bytes:14; //< Should this be samples instead?; limit is 16384
	uint8_t hz; //< Sample rate. 25 Hz = 25 samples per second = 150 bytes per second for accel/gyro only values = 300 bytes per second for combined accel+gyro values.
} __attribute__((packed));
