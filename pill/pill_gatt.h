// vi:noet:sw=4 ts=4

/* Copyright (c) 2013 Hello Inc. All Rights Reserved. */

#pragma once

#include <stdint.h>

#include <ble.h>

#define BLE_UUID_HELLO_BASE {0x23, 0xD1, 0xBC, 0xEA, 0x5F,              \
      0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00}

typedef void (*hlo_ble_write_handler)(ble_gatts_evt_write_t *);
typedef void (*hlo_ble_connect_handler)(void);

extern uint8_t hello_type;

/** This a data structure designed to be transmitted over BLE. The
    struct should be exactly 20 bytes, which is the maximum BLE packet
    size. */
struct hlo_ble_packet {
    union {
        struct {
            /* We have sequence numbers from 0-255. This implies that the
               maximum payload we can send is 4824 bytes: 17 bytes in the
               header = 1 sequence number, plus 0 bytes in the footer (since
               the footer is entirely a checksum) = 2 sequence numbers, plus
               4807 bytes for the body (19 bytes per packet) = 255 sequence
               numbers. */
            uint8_t sequence_number;

            union {
                struct {
                    uint8_t packet_count;
                    uint8_t data[18];
                } header;

                struct {
                    uint8_t data[19];
                } body;

                struct {
                    uint8_t sha19[19]; // First 19 bytes of SHA-1.
                } footer;
            };
        } __attribute__((packed));

        uint8_t bytes[20];
    };
};

void hlo_ble_init();

void hlo_ble_char_notify_add(uint16_t uuid);
void hlo_ble_char_indicate_add(uint16_t uuid);
void hlo_ble_char_write_request_add(uint16_t uuid, hlo_ble_write_handler write_handler, uint16_t max_buffer_size);
void hlo_ble_char_write_command_add(uint16_t uuid, hlo_ble_write_handler write_handler, uint16_t max_buffer_size);
void hlo_ble_char_read_add(uint16_t uuid, uint8_t* const value, uint16_t value_size);

uint16_t hlo_ble_get_value_handle(const uint16_t uuid);
uint16_t hlo_ble_get_connection_handle();

void hlo_ble_dispatch_write(ble_evt_t *event);

typedef void (*hlo_ble_notify_callback)();
void hlo_ble_notify(uint16_t characteristic_uuid, uint8_t* data, uint16_t length, hlo_ble_notify_callback callback);
void hlo_ble_on_ble_evt(ble_evt_t* event);
