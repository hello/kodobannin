// vi:noet:sw=4 ts=4

#pragma once

#include "hlo_ble_time.h"
#include "hlo_gatts.h"

#include "pb_decode.h"
#include "pb_encode.h"
#include "morpheus_ble.pb.h"

#define MSG_NO_MEMORY   "No memory\r\n"

enum {
    BLE_UUID_MORPHEUS_SVC = 0xFEE1,
};

void morpheus_ble_evt_handler(ble_evt_t* ble_evt);
void morpheus_ble_write_handler(ble_gatts_evt_write_t* event);
void morpheus_load_modules(void);
bool morpheus_ble_reply_protobuf(MorpheusCommand* morpheus_command);
bool morpheus_ble_reply_protobuf_error(uint32_t error_type);
