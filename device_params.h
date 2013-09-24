#pragma once

/*
 * Board GPIO defines
 */

// For EVT-1
#define GPIO_3v3_Enable  5
#define GPIO_HRS_PWM     2
#define GPIO_VIBE_PWM   30

// For PCA-1000 v2
/*
#define ASSERT_LED_PIN_NO      21
#define CONNECTED_LED_PIN_NO   22
#define ADVERTISING_LED_PIN_NO 23
*/

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
#define BLE_MANUFACTURER_NAME "H"
//#define BLE_MODEL_NUM         "EVT1-r3"
#define BLE_MANUFACTURER_ID   0x43110
#define BLE_ORG_UNIQUE_ID     0x1337

/*
 * BLE Bond Parameters
 */
#define FLASH_PAGE_BOND     255
#define FLASH_PAGE_SYS_ATTR 253

/*
 * BLE Connection Parameters
 */
// Advertising interval (in units of 0.625 ms)
#define APP_ADV_INTERVAL                     40
// Advertising timeout in units of seconds.
#define APP_ADV_TIMEOUT_IN_SECONDS           180

// Definition of 1 second, when 1 unit is 1.25 ms.
#define SECOND_1_25_MS_UNITS                    800
#define TWENTY_MS_1_25_MS_UNITS                 16

// Definition of 1 second, when 1 unit is 10 ms.
#define SECOND_10_MS_UNITS                   100
// Minimum acceptable connection interval (0.5 seconds),
// Connection interval uses 1.25 ms units
#define MIN_CONN_INTERVAL                    (uint16_t)(MSEC_TO_UNITS(11.25, UNIT_1_25_MS))          /**< Minimum acceptable connection interval (11.25 milliseconds). */
#define MAX_CONN_INTERVAL                    (uint16_t)(MSEC_TO_UNITS(15, UNIT_1_25_MS))             /**< Maximum acceptable connection interval (15 milliseconds). */

//#define MIN_CONN_INTERVAL                    (TWENTY_MS_1_25_MS_UNITS)
// Maximum acceptable connection interval (1 second), Connection interval uses 1.25 ms units.
//#define MAX_CONN_INTERVAL                    (2*TWENTY_MS_1_25_MS_UNITS)

// Slave latency. */
#define SLAVE_LATENCY                        0
// Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units
#define CONN_SUP_TIMEOUT                     (5*TWENTY_MS_1_25_MS_UNITS)

// Time from initiating event (connect or start of notification) to first
// time ble_gap_conn_param_update is called (5 seconds)
#define FIRST_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(100, APP_TIMER_PRESCALER)               /**< Time from the Connected event to first time sd_ble_gap_conn_param_update is called (100 milliseconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY        APP_TIMER_TICKS(500, APP_TIMER_PRESCALER)               /**< Time between each call to sd_ble_gap_conn_param_update after the first (500 milliseconds). */
//#define FIRST_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)
// Time between each call to ble_gap_conn_param_update after the first (30 seconds)
//#define NEXT_CONN_PARAMS_UPDATE_DELAY        APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)
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

