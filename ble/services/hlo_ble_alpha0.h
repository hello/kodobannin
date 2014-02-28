// vi:noet:sw=4 ts=4

/* Copyright (c) 2013 Hello Inc. All Rights Reserved. */

#pragma once

#define BLE_UUID_HELLO_ALPHA0_SVC 0xBA5E
#define BLE_UUID_HELLO_ALPHA0_CONTROL_CHAR 0xDEED
#define BLE_UUID_HELLO_ALPHA0_CONTROL_RESPONSE_CHAR 0xD00D
#define BLE_UUID_HELLO_ALPHA0_DATA_CHAR 0xFEED
#define BLE_UUID_HELLO_ALPHA0_DATA_RESPONSE_CHAR 0xF00D
#define BLE_UUID_HELLO_ALPHA0_GIT_DESCRIPTION_CHAR 0x617D

enum hlo_ble_command {
    HLO_BLE_COMMAND_SEND_DATA,
    HLO_BLE_COMMAND_DISCONNECT,
    HLO_BLE_COMMAND_SET_TIME,
};

struct hlo_ble_command_request {
    enum hlo_ble_command command;
    union /* command parameters */ {
        uint32_t timestamp; // for HLO_BLE_COMMAND_SET_TIME
    };
} __attribute__((packed));

void hlo_ble_alpha0_init();
