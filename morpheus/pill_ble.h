// vi:noet:sw=4 ts=4

#pragma once
#include "platform.h"
#include "hlo_ble_time.h"

enum {
    BLE_UUID_PILL_SVC = 0xE110,
};

enum pill_command_type {
    PILL_COMMAND_STOP_ACCELEROMETER = 0,
    PILL_COMMAND_START_ACCELEROMETER,
    PILL_COMMAND_CALIBRATE,
    PILL_COMMAND_DISCONNECT,
    PILL_COMMAND_SEND_DATA,
    PILL_COMMAND_GET_TIME,
    PILL_COMMAND_SET_TIME,
} __attribute__((packed));

struct pill_command
{
    enum pill_command_type command;
    union {
        struct hlo_ble_time set_time;
    };
} __attribute__((packed));

enum pill_stream_command_type {
    PILL_STREAM_COMMAND_STOP,
    PILL_STREAM_COMMAND_START,
} __attribute__((packed));

struct pill_stream_command
{
    enum pill_stream_command_type command;
} __attribute__((packed));

enum pill_data_response_type {
    PILL_DATA_RESPONSE_ACK,
    PILL_DATA_RESPONSE_MISSING,
} __attribute__((packed));

struct pill_data_response
{
    enum pill_data_response_type type;
    union {
        uint8_t acked_sequence_number;
        struct {
            uint8_t missing_packet_count;
            uint8_t missing_packet_sequence_numbers[18];
        };
    };
} __attribute__((packed));

void pill_ble_services_init(void);
void pill_ble_evt_handler(ble_evt_t* ble_evt);
void pill_ble_advertising_init(void);
void pill_ble_load_modules(void);
