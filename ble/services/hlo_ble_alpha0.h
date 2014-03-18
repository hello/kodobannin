// vi:noet:sw=4 ts=4

/* Copyright (c) 2013 Hello Inc. All Rights Reserved. */

#pragma once


#define BLE_UUID_HELLO_ALPHA0_SVC 0xBA5E
#define BLE_UUID_HELLO_ALPHA0_CONTROL_CHAR 0xDEED
#define BLE_UUID_HELLO_ALPHA0_CONTROL_RESPONSE_CHAR 0xD00D
#define BLE_UUID_HELLO_ALPHA0_DATA_CHAR 0xFEED
#define BLE_UUID_HELLO_ALPHA0_DATA_RESPONSE_CHAR 0xF00D
#define BLE_UUID_HELLO_ALPHA0_GIT_DESCRIPTION_CHAR 0x617D


enum hlo_ble_alpha0_command {
    HLO_BLE_ALPHA0_COMMAND_DISCONNECT = 0,
    HLO_BLE_ALPHA0_COMMAND_SET_TIME = 1,
    HLO_BLE_ALPHA0_COMMAND_SEND_DATA = 2,
} __attribute__((packed));

enum hlo_ble_alpha0_command_response {
    HLO_BLE_ALPHA0_COMMAND_RESPONSE_DISCONNECT_ACK = 0,
} __attribute__((packed));

/** Note that this struct should be a maximum of 20 bytes, which is
    the maximum BLE packet size. */
struct hlo_ble_alpha0_command_request {
    enum hlo_ble_alpha0_command command;
    union /* command parameters */ {
        uint32_t timestamp; // for HLO_BLE_COMMAND_SET_TIME
    };
} __attribute__((packed));


enum hlo_ble_alpha0_data_response_command {
    HLO_BLE_ALPHA0_DATA_RESPONSE_ACK_PACKET = 0,
    HLO_BLE_ALPHA0_DATA_RESPONSE_RESEND_PACKET = 1,
};

/** Note that this struct should be a maximum of 20 bytes, which is
    the maximum BLE packet size. */
struct hlo_ble_alpha0_data_response {
    enum hlo_ble_alpha0_data_response_command command;
    union /* command parameters */ {
        uint8_t sequence_number; // for HLO_BLE_ALPHA0_DATA_RESPONSE_ACK_PACKET and HLO_BLE_ALPHA0_DATA_RESPONSE_RESEND_PACKET
    };
} __attribute__((packed));


void hlo_ble_alpha0_init();
