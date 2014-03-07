// vi:noet:sw=4 ts=4

/* Copyright (c) 2013 Hello Inc. All Rights Reserved. */

#include <stdint.h>

#include <app_error.h>
#include <ble_types.h>
#include <string.h>

#include "git_description.h"
#include "hlo_ble.h"
#include "hlo_ble_alpha0.h"
#include "util.h"

static uint16_t _alpha0_service_handle;

static void
_control_write_handler(ble_gatts_evt_write_t *event)
{

}

static void
_data_response_write_handler(ble_gatts_evt_write_t *event)
{

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
