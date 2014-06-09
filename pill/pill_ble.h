// vi:noet:sw=4 ts=4

#pragma once

enum {
    BLE_UUID_PILL_SVC = 0xE110,
};

enum pill_command_type {
    PILL_COMMAND_STOP_ACCELEROMETER,
    PILL_COMMAND_START_ACCELEROMETER,
    PILL_COMMAND_CALIBRATE,
    PILL_COMMAND_DISCONNECT,
    PILL_COMMAND_SEND_DATA,
} __attribute__((packed));

struct pill_command
{
    union {
        enum pill_command_type command;
        uint8_t bytes[1];
    };
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

void pill_ble_services_init();
void pill_ble_evt_handler(ble_evt_t* ble_evt);
