#pragma once

#include <stdint.h>

#include <nrf51.h>
#include <nrf51_bitfields.h>

#include "platform.h"

enum {
    APP_TIMER_PRESCALER = 0,
    APP_TIMER_MAX_TIMERS = 6,
    APP_TIMER_OP_QUEUE_SIZE = 2,
};

/*
 * BLE Advertising Strings
 */
#define BLE_DEVICE_NAME       "Sense"
#define BLE_MANUFACTURER_NAME "Hello Inc."
#define BLE_MODEL_NUM         FW_VERSION_STRING
#define BLE_MANUFACTURER_ID   0x43110
#define BLE_ORG_UNIQUE_ID     0x1337

#define BONDING_REQUIRED

#ifdef BONDING_REQUIRED
//#define IN_MEMORY_BONDING
#endif
 
#define PROTO_REPLY   // use protobuf for all reply, even in 0xD00D
#define PROTOBUF_VERSION		(0)
#define PROTOBUF_MAX_LEN        (100)

#ifdef DEBUG_SERIAL
#define PB_NO_ERRMSG
#endif

/*
 * BLE Connection Parameters
 */
// Advertising interval (in units of 0.625 ms)
#define APP_ADV_INTERVAL                     320

// Advertising timeout in units of seconds.
#define APP_ADV_TIMEOUT_IN_SECONDS           (0)

// Definition of 1 second, when 1 unit is 1.25 ms.
#define SECOND_1_25_MS_UNITS                    800
#define TWENTY_MS_1_25_MS_UNITS                 16

// Definition of 1 second, when 1 unit is 10 ms.
#define SECOND_10_MS_UNITS                   100
// Minimum acceptable connection interval (0.5 seconds),
// Connection interval uses 1.25 ms units

#define MIN_CONN_INTERVAL                    (2 * TWENTY_MS_1_25_MS_UNITS)
// Maximum acceptable connection interval (1 second), Connection interval uses 1.25 ms units.
#define MAX_CONN_INTERVAL                    (3 * TWENTY_MS_1_25_MS_UNITS)

// Slave latency. */
#define SLAVE_LATENCY                        1
// Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units
#define CONN_SUP_TIMEOUT                     (4 * MAX_CONN_INTERVAL * (SLAVE_LATENCY + 1))


// Time from initiating event (connect or start of notification) to first
// time ble_gap_conn_param_update is called (5 seconds)
#define FIRST_CONN_PARAMS_UPDATE_DELAY       APP_TIMER_TICKS(1000, APP_TIMER_PRESCALER)
// Time between each call to ble_gap_conn_param_update after the first (30 seconds)
#define NEXT_CONN_PARAMS_UPDATE_DELAY        APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)
// Number of attempts before giving up the connection parameter negotiation
#define MAX_CONN_PARAMS_UPDATE_COUNT         3

// Timeout for Pairing Request or Security Request (in seconds)
#define SEC_PARAM_TIMEOUT                    30

#ifdef BONDING_REQUIRED
// Perform bonding.
#define SEC_PARAM_BOND                       1
#else
#define SEC_PARAM_BOND                       0
#endif

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

#define TX_POWER_LEVEL (0)

#define APP_PILL_PAIRING_TIMEOUT_INTERVAL	 (APP_TIMER_TICKS(60000, APP_TIMER_PRESCALER))
#define BLE_BOOT_RETRY_INTERVAL              (APP_TIMER_TICKS(2500, APP_TIMER_PRESCALER))

//fatory app allows more capabilities
#define FACTORY_APP
//verbose app shows more txt for debugging
#define VERBOSE_DEBUG

