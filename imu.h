// vi:sw=4:ts=4

#pragma once

enum imu_sensor_set {
    IMU_SENSORS_DISABLED = 0,
    IMU_SENSORS_ACCEL = 1,
    IMU_SENSORS_GYRO = 2,
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

struct imu_settings {
	uint16_t wom_threshold; // in microgravities
    enum imu_hz inactive_sample_rate;
    enum imu_sensor_set active_sensors;
	enum imu_hz active_sample_rate;
	bool active;
};

/// Initializes the IMU.
/** This immediately sets the IMU into sleep (deactivated) mode, with some default values specified in the _settings variable in imu.c. You will get an interrupt low trigger on IMU_INT when the IMU detects motion.  */
void imu_init(enum SPI_Channel channel);

uint16_t imu_accel_reg_read(uint8_t *buf);
uint16_t imu_read_regs(uint8_t *buf);
void imu_set_sample_rate(uint8_t hz);

/// Given the enum imu_hz passed to it, returns the sampling interval (a.k.a. sampling period or sampling time, in milliseconds).
unsigned imu_get_sampling_interval(enum imu_hz);

void imu_set_sensors(enum imu_sensor_set sensors);
void imu_get_settings(struct imu_settings* settings);

uint16_t imu_fifo_bytes_available();
uint16_t imu_fifo_read(uint16_t count, uint8_t *buf);
void imu_fifo_clear();

bool imu_is_active();

/// Activate the IMU by waking it up from sleep mode.
void imu_activate();
/// Deactivate the IMU by putting it into sleep mode.
void imu_deactivate();
void imu_clear_interrupt_status();
void imu_wom_disable();

uint32_t imu_timer_ticks_from_sample_rate(uint8_t hz);

#define IMU_FIFO_CAPACITY 4096 // Must be 512, 1024, 2048 or 4096
