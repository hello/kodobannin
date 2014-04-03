// vi:noet:sw=4 ts=4

#pragma once

#include <stdint.h>

/*
 * Board GPIO defines
 */
extern const uint32_t GPIO_3v3_Enable;
extern const uint32_t GPIO_VIBE_PWM;

extern const uint32_t IMU_SPI_MOSI;
extern const uint32_t IMU_SPI_MISO;
extern const uint32_t IMU_SPI_SCLK;
extern const uint32_t IMU_SPI_nCS;
extern const uint32_t IMU_INT;

extern const uint32_t MOSI;
extern const uint32_t MISO;
extern const uint32_t SCLK;
extern const uint32_t GPS_nCS;
extern const uint32_t FLASH_nCS;

extern const uint32_t GPIO_HRS_PWM_G1;
extern const uint32_t GPIO_HRS_PWM_G2;

extern const uint32_t GPS_ON_OFF;

extern const uint32_t RTC_SDA;
extern const uint32_t RTC_SCL;
extern const uint32_t RTC_INT;

extern const uint32_t GAUGE_SDA;
extern const uint32_t GAUGE_SCL;
extern const uint32_t GAUGE_INT;

extern const uint32_t STATUS0;
extern const uint32_t STATUS1;

extern const uint32_t HRS_SENSE_FILT;
extern const uint32_t HRS_SENSE_RAW;

extern const uint32_t HRS_RAW_ADC;
extern const uint32_t HRS_FILT_ADC;
extern const uint32_t HRS_ADC;

extern const uint32_t SERIAL_TX_PIN;
extern const uint32_t SERIAL_RX_PIN;
extern const uint32_t SERIAL_CTS_PIN;
extern const uint32_t SERIAL_RTS_PIN;

extern const uint32_t nBATT_CHG;

/*
 * BLE Advertising Strings
 */
extern const char* const BLE_DEVICE_NAME;
extern const char* const BLE_MANUFACTURER_NAME;
extern const char* const BLE_MODEL_NUM;
extern const uint32_t BLE_MANUFACTURER_ID;
extern const uint32_t BLE_ORG_UNIQUE_ID;

/*
 * BLE Bond Parameters
 */
extern const uint32_t FLASH_PAGE_BOND;
extern const uint32_t FLASH_PAGE_SYS_ATTR;

/*
 * BLE Connection Parameters
 */
// Advertising interval (in units of 0.625 ms)
extern const uint32_t APP_ADV_INTERVAL;
// Advertising timeout in units of seconds.
extern const uint32_t APP_ADV_TIMEOUT_IN_SECONDS;

// Definition of 1 second, when 1 unit is 1.25 ms.
extern const uint32_t SECOND_1_25_MS_UNITS;
enum {
    TWENTY_MS_1_25_MS_UNITS = 16,
};
// extern const uint32_t TWENTY_MS_1_25_MS_UNITS;

// Definition of 1 second, when 1 unit is 10 ms.
extern const uint32_t SECOND_10_MS_UNITS;
// Minimum acceptable connection interval (0.5 seconds) & maximum acceptable connection interval (1 second). Connection interval uses 1.25 ms units.
enum {
    MIN_CONN_INTERVAL = TWENTY_MS_1_25_MS_UNITS,
    MAX_CONN_INTERVAL = 2*TWENTY_MS_1_25_MS_UNITS,
};

// Slave latency. */
extern const uint32_t SLAVE_LATENCY;
// Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units
extern const uint32_t CONN_SUP_TIMEOUT;

// Time from initiating event (connect or start of notification) to first
// time ble_gap_conn_param_update is called (5 seconds)
// extern const uint32_t FIRST_CONN_PARAMS_UPDATE_DELAY;
// Time between each call to ble_gap_conn_param_update after the first (30 seconds)
extern const uint32_t NEXT_CONN_PARAMS_UPDATE_DELAY;
// Number of attempts before giving up the connection parameter negotiation
extern const uint32_t MAX_CONN_PARAMS_UPDATE_COUNT;

// Timeout for Pairing Request or Security Request (in seconds)
extern const uint32_t SEC_PARAM_TIMEOUT;
// Perform bonding.
extern const uint32_t SEC_PARAM_BOND;
// Man In The Middle protection not required.
extern const uint32_t SEC_PARAM_MITM;
// No I/O capabilities
extern const uint32_t SEC_PARAM_IO_CAPABILITIES;
// Out Of Band data not available
extern const uint32_t SEC_PARAM_OOB;
// Minimum encryption key size
extern const uint32_t SEC_PARAM_MIN_KEY_SIZE;
// Maximum encryption key size
extern const uint32_t SEC_PARAM_MAX_KEY_SIZE;

/*
 * Intervals for timed activities
 */
// Battery level measurement interval (ticks).
extern const uint32_t BATTERY_LEVEL_MEAS_INTERVAL;
