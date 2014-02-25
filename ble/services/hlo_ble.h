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

void hlo_ble_init();

void hlo_ble_char_notify_add(uint16_t uuid);
void hlo_ble_char_indicate_add(uint16_t uuid);
void hlo_ble_char_write_request_add(uint16_t uuid, hlo_ble_write_handler write_handler, uint16_t max_buffer_size);
void hlo_ble_char_write_command_add(uint16_t uuid, hlo_ble_write_handler write_handler, uint16_t max_buffer_size);
void hlo_ble_char_read_add(uint16_t uuid, uint8_t* const value, uint16_t value_size);

uint16_t hlo_ble_get_value_handle(const uint16_t uuid);

void hlo_ble_dispatch_write(ble_evt_t *event);
