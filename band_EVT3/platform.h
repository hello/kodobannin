// vi:noet:sw=4 ts=4

#pragma once

#include <stdint.h>

#include <nrf51.h>
#include <nrf51_bitfields.h>

/*
 * Board GPIO defines
 */
#define GPIO_3v3_Enable    13
#define GPIO_VIBE_PWM      0

#define IMU_SPI_MOSI       14
#define IMU_SPI_MISO       3 // was 22 in EVT2
#define IMU_SPI_SCLK       19
#define IMU_SPI_nCS        17
#define IMU_INT             2

#define MOSI         8
#define MISO        11
#define SCLK        15
#define GPS_nCS     12
#define FLASH_nCS   16

// For EVT-3
#define GPIO_HRS_PWM_G1   21
#define GPIO_HRS_PWM_G2   23

#define GPS_ON_OFF         7

#define RTC_SDA           28
#define RTC_SCL           24
#define RTC_INT           29

#define GAUGE_SDA         30
#define GAUGE_SCL         25
#define GAUGE_INT         31

#define STATUS0            4
#define STATUS1            6

#define HRS_SENSE_FILT     1
#define HRS_SENSE_RAW     26

#define HRS_RAW_ADC       ADC_CONFIG_PSEL_AnalogInput0
#define HRS_FILT_ADC      ADC_CONFIG_PSEL_AnalogInput2
#define HRS_ADC HRS_FILT_ADC

#define SERIAL_TX_PIN     20
#define SERIAL_RX_PIN      5
#define SERIAL_CTS_PIN     0
#define SERIAL_RTS_PIN     0

#define nBATT_CHG   22

/*
 * Timer config
 */

#define APP_TIMER_PRESCALER     0
#define APP_TIMER_MAX_TIMERS    4
#define APP_TIMER_OP_QUEUE_SIZE 5

/*
 * BLE Advertising Strings
 */
#define BLE_DEVICE_NAME       "Band"
#define BLE_MANUFACTURER_NAME "Hello Inc."
#define BLE_MODEL_NUM         "EVT3"
#define BLE_MANUFACTURER_ID   0x43110
#define BLE_ORG_UNIQUE_ID     0x1337

/*
 * BLE Bond Parameters
 */
#define FLASH_PAGE_BOND     253 // FLASH_PAGE_SYS_ATTR+2
#define FLASH_PAGE_SYS_ATTR 251 // 3xEC00: BLE_BONDMNGR_SYS_ATTR/BLE_FLASH_PAGE_SIZE (1024)

/*
 * BLE Connection Parameters
 */
// Advertising interval (in units of 0.625 ms)
#define APP_ADV_INTERVAL                     800//40
// Advertising timeout in units of seconds.
#define APP_ADV_TIMEOUT_IN_SECONDS           180

// Definition of 1 second, when 1 unit is 1.25 ms.
#define SECOND_1_25_MS_UNITS                    800
#define TWENTY_MS_1_25_MS_UNITS                 16

// Definition of 1 second, when 1 unit is 10 ms.
#define SECOND_10_MS_UNITS                   100
// Minimum acceptable connection interval (0.5 seconds),
// Connection interval uses 1.25 ms units
#define MIN_CONN_INTERVAL                    (TWENTY_MS_1_25_MS_UNITS)
// Maximum acceptable connection interval (1 second), Connection interval uses 1.25 ms units.
#define MAX_CONN_INTERVAL                    (2*TWENTY_MS_1_25_MS_UNITS)

// Slave latency. */
#define SLAVE_LATENCY                        0
// Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units
#define CONN_SUP_TIMEOUT                     (5*TWENTY_MS_1_25_MS_UNITS)

// Time from initiating event (connect or start of notification) to first
// time ble_gap_conn_param_update is called (5 seconds)
#define FIRST_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)
// Time between each call to ble_gap_conn_param_update after the first (30 seconds)
#define NEXT_CONN_PARAMS_UPDATE_DELAY        APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)
// Number of attempts before giving up the connection parameter negotiation
#define MAX_CONN_PARAMS_UPDATE_COUNT         3

// Timeout for Pairing Request or Security Request (in seconds)
#define SEC_PARAM_TIMEOUT                    30
// Perform bonding.
#define SEC_PARAM_BOND                       1
// Man In The Middle protection not required.
#define SEC_PARAM_MITM                       0
// No I/O capabilities
#define SEC_PARAM_IO_CAPABILITIES            BLE_GAP_IO_CAPS_NONE
// Out Of Band data not available
#define SEC_PARAM_OOB                        0
// Minimum encryption key size
#define SEC_PARAM_MIN_KEY_SIZE               7
// Maximum encryption key size
#define SEC_PARAM_MAX_KEY_SIZE               16

/*
 * Intervals for timed activities
 */
// Battery level measurement interval (ticks).
#define BATTERY_LEVEL_MEAS_INTERVAL          APP_TIMER_TICKS(200000, APP_TIMER_PRESCALER)
