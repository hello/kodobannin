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

/** This a data structure designed to be transmitted over BLE. The
    struct should be exactly 20 bytes, which is the maximum BLE packet
    size. */
struct hlo_ble_alpha0_data_packet {
    /* We have sequence numbers from 0-255. This implies that the
       maximum payload we can send is 4824 bytes: 17 bytes in the
       header = 1 sequence number, plus 0 bytes in the footer (since
       the footer is entirely a checksum) = 2 sequence numbers, plus
       4807 bytes for the body (19 bytes per packet) = 255 sequence
       numbers. */
    uint8_t sequence_number;

    union {
        struct {
            uint16_t size;
            uint8_t data[17];
        } header;

        struct {
            uint8_t data[19];
        } body;

        struct {
            uint8_t sha19[19]; // First 19 bytes of SHA-1.
        } footer;
    };
} __attribute__((packed));

void hlo_ble_alpha0_init();
