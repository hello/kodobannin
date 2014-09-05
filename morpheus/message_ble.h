#pragma once
#include "message_base.h"


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

MSG_Base_t* MSG_BLE_Base(MSG_Central_t* parent);
MSG_Status route_data_to_cc3200(const MSG_Data_t* data);
//MSG_Status process_pending_pill_piairing_request(const char* account_id);
MSG_Status process_pending_pill_piairing_request(MSG_Data_t * account_id_page);
MSG_Status send_remove_pill_notification(const char* pill_id);

