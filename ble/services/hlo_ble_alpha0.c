// vi:noet:sw=4 ts=4

/* Copyright (c) 2013 Hello Inc. All Rights Reserved. */

#include <stdint.h>

#include <app_error.h>
#include <ble_types.h>

#include "hlo_ble.h"
#include "hlo_ble_alpha0.h"
#include "util.h"

static uint16_t _alpha0_service_handle;

void hlo_ble_alpha0_init()
{
  ble_uuid_t ble_uuid = {
    .type = hello_type,
    .uuid = BLE_UUID_HELLO_ALPHA0_SVC,
  };

  uint32_t err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &_alpha0_service_handle);
  APP_ERROR_CHECK(err_code);
}
