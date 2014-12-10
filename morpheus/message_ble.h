#pragma once
#include "message_base.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "morpheus_ble.pb.h"


struct pill_pairing_request {
	MSG_Data_t* account_id;
	MSG_Data_t* device_id;
};


typedef struct{
    enum{
        BLE_PING=0,
        BLE_ACK_DEVICE_REMOVED,
        BLE_ACK_DEVICE_ADDED
    }cmd;
    union{
        uint64_t pill_uid;
    }param;
}MSG_BLECommand_t;

typedef enum {
    BOOT_CHECK = 0,
    BOOT_SYNC_DEVICE_ID,
    BOOT_COMPLETED
} boot_status;

MSG_Base_t* MSG_BLE_Base(MSG_Central_t* parent);
MSG_Status message_ble_route_data_to_cc3200(MSG_Data_t* data);
MSG_Status message_ble_pill_pairing_begin(MSG_Data_t* account_id_page);
MSG_Status message_ble_remove_pill(MSG_Data_t* pill_id_page);
void message_ble_reset();
void message_ble_on_protobuf_command(MSG_Data_t* data_page, MorpheusCommand* command);

