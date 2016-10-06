// vi:noet:sw=4 ts=4

#pragma once

#include "spi_nor.h"

#include "imu_data.h"
#include "message_base.h"

struct sensor_data_header;

enum {
    IMU_FIFO_CAPACITY_BYTES = 192, // 4096, // Must be 512, 1024, 2048 or 4096
	IMU_FIFO_CAPACITY_WORDS = IMU_FIFO_CAPACITY_BYTES/2,
	IMU_FIFO_CAPACITY_SAMPLES = IMU_FIFO_CAPACITY_WORDS/3,
};


/* See README_IMU.md for an introduction to the IMU, and vocabulary
   that you may need to understand the rest of this. */

/// Initializes the IMU.
/** This immediately sets the IMU into sleep (deactivated) mode, with some default values specified in the _settings variable in imu.c. You will get an interrupt low trigger on IMU_INT when the IMU detects motion.  */
int32_t imu_init_low_power(enum SPI_Channel channel, enum SPI_Mode mode, 
            uint8_t miso, uint8_t mosi, 
            uint8_t sclk, uint8_t nCS, 
            enum imu_hz sampling_rate,
            enum imu_accel_range acc_range, uint16_t wom_threshold);

uint16_t imu_accel_reg_read(uint16_t *values);

void imu_set_accel_range(enum imu_accel_range range);

void imu_enable_all_axis();

/// Given the enum imu_hz passed to it, returns the sampling interval (a.k.a. sampling period or sampling time, in milliseconds).
unsigned imu_get_sampling_interval(enum imu_hz);

void imu_set_accel_freq(enum imu_hz sampling_rate);

// Enable high resolution
void imu_enable_hres();

// Disable high resolution
void imu_disable_hres();

// Enable low power mode
void imu_lp_enable();

// Disable Low power mode
void imu_lp_disable();

// Reset high pass filter
void imu_reset_hp_filter();

// Enable IMU FIFO
void imu_fifo_enable();

// Disable IMU FIFO
void imu_fifo_disable();

// Read from FIFO
uint8_t imu_handle_fifo_read(uint16_t* values);

// Set FIFO mode
void imu_set_fifo_mode(enum imu_fifo_mode fifo_mode, uint8_t fifo_trigger, uint8_t wtm_threshold);

// Read FIFO source register
uint8_t imu_read_fifo_src_reg();

/// Activate the IMU by waking it up from sleep mode.
void imu_enter_normal_mode();

/// Deactivate the IMU by putting it into sleep mode. This also clears the the IMU interrupt.
void imu_enter_low_power_mode();

// Clear IMU interrupt on PIN1
uint8_t imu_clear_interrupt_status();

// Set IMU threshold
void imu_wom_set_threshold(uint16_t microgravities);

// Reset IMU
void imu_reset();

void imu_spi_enable();

void imu_power_on();

void imu_power_off();

// Enable self-test mode
void imu_self_test_enable();

// Disable self-test mode
void imu_self_test_disable();

// perform imu self-test
int imu_self_test(void);

// Enable interrupts
void imu_enable_intr();

// Disable IMU interrupts
void imu_disable_intr();

// Check if data read from IMU is within threshold
bool imu_data_within_thr(int16_t value);
int32_t imu_init_simple(enum SPI_Channel channel,
		enum SPI_Mode mode,
		uint8_t miso, uint8_t mosi, uint8_t sclk, uint8_t nCS);
//
int32_t imu_tap_enable(uint32_t tap_pin);
