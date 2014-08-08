// vi:noet:sw=4 ts=4

#pragma once

#include "hlo_ble_time.h"
#include "morpheus_gatt.h"

enum {
    BLE_UUID_MORPHEUS_SVC = 0xFEE1,
};

enum morpheus_command_type {
    MORPHEUS_COMMAND_SET_TIME = 0,
    MORPHEUS_COMMAND_GET_TIME,
    MORPHEUS_COMMAND_SET_WIFI_ENDPOINT,
    MORPHEUS_COMMAND_GET_WIFI_ENDPOINT,
    MORPHEUS_COMMAND_SET_ALARMS,
    MORPHEUS_COMMAND_GET_ALARMS,
    MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE,
    MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE,
    MORPHEUS_COMMAND_START_WIFISCAN,
    MORPHEUS_COMMAND_STOP_WIFISCAN,
    MORPHEUS_COMMAND_GET_DEVICE_ID,
    
} __attribute__((packed));

struct morpheus_command
{
    enum morpheus_command_type command;
    union {
        struct hlo_ble_packet payload;   // Now the command format is [command_type][optional: [seq#][optional:total][data] ]
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
void morpheus_load_modules(void);
void morpheus_ble_transmission_layer_init();
