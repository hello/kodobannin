// vi:noet:sw=4 ts=4

/* Copyright (c) 2013 Hello Inc. All Rights Reserved. */

#include <stdint.h>

#include <app_error.h>
#include <ble_types.h>
#include <string.h>

#include "git_description.h"
#include "hlo_ble.h"
#include "hlo_ble_alpha0.h"
#include "hlo_fs.h"
#include "sha1.h"
#include "util.h"

static uint16_t _alpha0_service_handle;

#define SIMULATE_DATA

static int32_t
_fs_read(enum HLO_FS_Partition_ID id, uint32_t len, uint8_t *data, HLO_FS_Page_Range *range)
{
#ifdef SIMULATE_DATA
    static uint8_t counter;

    if(counter == 6) {
        counter = 0;
        return 0;
    }

    uint32_t i = 0;
    uint8_t c = counter;

    for(i = 0; i < len; i++) {
        data[i] = c++;
    }

    counter++;

    return (int32_t)len;
#else
    return hlo_fs_read(id, len, data, range);
#endif
}

static void
_send_sensor_data()
{
    const unsigned HLO_FS_PAGE_SIZE = 256;

    uint8_t data[HLO_FS_PAGE_SIZE];

    static HLO_FS_Page_Range _page_range;
    int32_t bytes_read = _fs_read(HLO_FS_Partition_Data, HLO_FS_PAGE_SIZE, data, &_page_range);

    if(bytes_read != 0) {
        hlo_ble_notify(BLE_UUID_HELLO_ALPHA0_DATA_CHAR, data, bytes_read, _send_sensor_data);
    }
}

static void
_control_write_handler(ble_gatts_evt_write_t *event)
{
    typedef struct hlo_ble_alpha0_command_request request_t;

    request_t* request = (request_t*)event->data;

    switch(request->command) {
    case HLO_BLE_ALPHA0_COMMAND_DISCONNECT:
        // send HLO_BLE_ALPHA0_COMMAND_RESPONSE_DISCONNECT_ACK
        break;
    case HLO_BLE_ALPHA0_COMMAND_SET_TIME:
        break;
    case HLO_BLE_ALPHA0_COMMAND_SEND_DATA:
        _send_sensor_data();
        break;
    }
}

static void
_data_response_write_handler(ble_gatts_evt_write_t *event)
{
    typedef struct hlo_ble_alpha0_data_response response_t;

    response_t* response = (response_t*)event->data;

    switch(response->command) {
    case HLO_BLE_ALPHA0_DATA_RESPONSE_ACK_PACKET:
        // [TODO]
        break;
    case HLO_BLE_ALPHA0_DATA_RESPONSE_RESEND_PACKET:
        // [TODO]
        break;
    }
}

void hlo_ble_alpha0_init()
{
    ble_uuid_t ble_uuid = {
        .type = hello_type,
        .uuid = BLE_UUID_HELLO_ALPHA0_SVC,
    };

    uint32_t err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &_alpha0_service_handle);
    APP_ERROR_CHECK(err_code);

    hlo_ble_char_read_add(BLE_UUID_HELLO_ALPHA0_GIT_DESCRIPTION_CHAR,
                          (uint8_t* const)GIT_DESCRIPTION,
                          strlen(GIT_DESCRIPTION));
    hlo_ble_char_write_request_add(BLE_UUID_HELLO_ALPHA0_CONTROL_CHAR, _control_write_handler, 5);
    hlo_ble_char_indicate_add(BLE_UUID_HELLO_ALPHA0_CONTROL_RESPONSE_CHAR);
    hlo_ble_char_notify_add(BLE_UUID_HELLO_ALPHA0_DATA_CHAR);
    hlo_ble_char_write_command_add(BLE_UUID_HELLO_ALPHA0_DATA_RESPONSE_CHAR, _data_response_write_handler, 5);
}
