// vi:noet:sw=4 ts=4

#pragma once

#include "hlo_ble_time.h"

enum {
    BLE_UUID_MORPHEUS_SVC = 0xEEE0,
};

enum morpheus_command_type {
    MORPHEUS_COMMAND_STOP_ACCELEROMETER = 0,
    MORPHEUS_COMMAND_START_ACCELEROMETER,
    MORPHEUS_COMMAND_CALIBRATE,
    MORPHEUS_COMMAND_DISCONNECT,
    MORPHEUS_COMMAND_SEND_DATA,
    MORPHEUS_COMMAND_GET_TIME,
    MORPHEUS_COMMAND_SET_TIME,
} __attribute__((packed));

struct morpheus_command
{
    enum morpheus_command_type command;
    union {
        struct hlo_ble_time set_time;
    };
} __attribute__((packed));


enum morpheus_data_response_type {
    PILL_DATA_RESPONSE_ACK,
    PILL_DATA_RESPONSE_MISSING,
} __attribute__((packed));


struct morpheus_data_response
{
    enum morpheus_data_response_type type;
    union {
        uint8_t acked_sequence_number;
        struct {
            uint8_t missing_packet_count;
            uint8_t missing_packet_sequence_numbers[18];
        };
    };
} __attribute__((packed));


void morpheus_ble_services_init(void);
void morpheus_ble_evt_handler(ble_evt_t* ble_evt);
void morpheus_ble_advertising_init(void);
void morpheus_ble_load_modules(void);
