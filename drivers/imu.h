// vi:noet:sw=4 ts=4

#pragma once

#include "spi_nor.h"

#include "imu_data.h"
#include "message_base.h"

struct sensor_data_header;

enum {
    IMU_FIFO_CAPACITY = 4096, // Must be 512, 1024, 2048 or 4096
};

typedef void(*imu_data_ready_callback_t)(uint16_t fifo_bytes_available);
typedef void(*imu_wom_callback_t)(struct sensor_data_header* header);

struct imu_settings {
	uint16_t wom_threshold; // in microgravities
    enum imu_hz inactive_sample_rate;
    enum imu_sensor_set active_sensors;
	enum imu_hz active_sample_rate;
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

/// Initializes the IMU.
/** This immediately sets the IMU into sleep (deactivated) mode, with some default values specified in the _settings variable in imu.c. You will get an interrupt low trigger on IMU_INT when the IMU detects motion.  */
int32_t imu_init(enum SPI_Channel channel, enum SPI_Mode mode, uint8_t miso, uint8_t mosi, uint8_t sclk, uint8_t nCS);
MSG_Base_t * imu_init_base(enum SPI_Channel channel, enum SPI_Mode mode, uint8_t miso, uint8_t mosi, uint8_t sclk, uint8_t nCS, MSG_Central_t * central);

uint16_t imu_accel_reg_read(uint8_t *buf);
uint16_t imu_read_regs(uint8_t *buf);
void imu_set_sample_rate(enum imu_hz hz);

void imu_set_accel_range(enum imu_accel_range range);
void imu_set_gyro_range(enum imu_gyro_range range);

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
/// Deactivate the IMU by putting it into sleep mode. This also clears the the IMU interrupt.
void imu_deactivate();
uint8_t imu_clear_interrupt_status();
void imu_wom_disable();

void imu_set_data_ready_callback(imu_data_ready_callback_t callback);
void imu_set_wom_callback(imu_wom_callback_t callback);

void imu_printf_wom_callback(struct sensor_data_header* header);
void imu_printf_data_ready_callback(uint16_t fifo_bytes_available);

void imu_calibrate_zero();
