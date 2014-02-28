// vi:noet:sw=4 ts=4

/* Copyright (c) 2013 Hello Inc. All Rights Reserved. */

#include "hlo_ble_demo.h"

#include <stdlib.h>
#include <string.h>
#include <app_error.h>
#include <ble_gatts.h>
#include <nordic_common.h>
#include <ble_srv_common.h>
#include <app_util.h>
#include <util.h>
#include <ble.h>

#include "git_description.h"

static uint16_t                 _service_handle;
static uint16_t                 _conn_handle;
static hlo_ble_connect_handler  _conn_handler;
static hlo_ble_connect_handler  _disconn_handler;

typedef uint32_t (*notify_data_send_cb)(const uint8_t *data, const uint16_t len);
static notify_data_send_cb data_cb = NULL;
static uint8_t *data_cb_data;
static uint16_t data_cb_len;

static void
_default_write_handler(ble_gatts_evt_write_t *event) {
    DEBUG("Default write handler called for 0x", event->handle);
}

static void
_default_conn_handler(void) {
    PRINTS("Default conn handler called\r\n");
}


void
hlo_ble_demo_on_ble_evt(ble_evt_t *event)
{
    switch (event->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            _conn_handle = event->evt.gap_evt.conn_handle;
			DEBUG("Connect from MAC: ", event->evt.gap_evt.params.connected.peer_addr.addr);

            _conn_handler();
            break;

        case BLE_GAP_EVT_DISCONNECTED:
			DEBUG("Disconnect from MAC: ", event->evt.gap_evt.params.disconnected.peer_addr.addr);
			DEBUG("Disconnect reason: ", event->evt.gap_evt.params.disconnected.reason);

            _conn_handle = BLE_CONN_HANDLE_INVALID;
            _disconn_handler();
            break;

        case BLE_GATTS_EVT_WRITE:
            hlo_ble_dispatch_write(event);
            break;

        case BLE_EVT_TX_COMPLETE:
            //PRINTS("BUF AVAIL\n");
            if (data_cb) {
                notify_data_send_cb cb = data_cb;
                data_cb = NULL;
                cb(data_cb_data, data_cb_len);

            }

            break;

        default:
            DEBUG("Hello Demo event unhandled: 0x", event->header.evt_id);
            break;
    };
}

uint32_t
hlo_ble_demo_data_send_blocking(const uint8_t *data, const uint16_t len) {
    uint32_t err;
    int32_t hvx_len;
    uint16_t pkt_len;
    ble_gatts_hvx_params_t hvx_params;
    uint8_t *ptr;

    data_cb = NULL;

    //todo: return verbose errors instead of 0
    if (_conn_handle == BLE_CONN_HANDLE_INVALID)
        return 0;

    hvx_len = len;
    ptr = (uint8_t *)data;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle   = hlo_ble_get_value_handle(BLE_UUID_DATA_CHAR);
    hvx_params.type     = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset   = 0;

    // send all the data
    while (hvx_len > 0) {

        // cap packet size at 20
        if (hvx_len >= 20)
            pkt_len = 20;
        else
            pkt_len = hvx_len;

        // configure data to send
        hvx_params.p_len  = &pkt_len;
        hvx_params.p_data = ptr;

        // send notification with data
        err = sd_ble_gatts_hvx(_conn_handle, &hvx_params);
        if (err == NRF_SUCCESS) {
            hvx_len -= pkt_len;
            ptr += pkt_len;
            //DEBUG("SENT PKT ", hvx_len);
        } else if (err == BLE_ERROR_NO_TX_BUFFERS) {
            //DEBUG("NEED BUF, ", hvx_len);
            // so we cant wait here because we're blocking the softdevice from actually sending
            // right now, we return an in progress indicator, and request that we be called back
            // with our in-progress data ptr and bytes
            data_cb = &hlo_ble_demo_data_send_blocking;
            data_cb_data = ptr;
            data_cb_len = hvx_len;
            return -1;
        } else {
            return err;
        }
    }

    return NRF_SUCCESS;
}

uint16_t
hlo_ble_demo_data_send(const uint8_t *data, const uint16_t len) {
    uint32_t err_code;
    uint16_t hvx_len;
    ble_gatts_hvx_params_t hvx_params;

    //todo: return verbose errors instead of 0
    if (_conn_handle == BLE_CONN_HANDLE_INVALID)
        return 0;

    hvx_len = len;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle   = hlo_ble_get_value_handle(BLE_UUID_DATA_CHAR);
    hvx_params.type     = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset   = 0;
    hvx_params.p_len    = &hvx_len;
    hvx_params.p_data   = (uint8_t *)data;

    err_code = sd_ble_gatts_hvx(_conn_handle, &hvx_params);
    if (err_code == NRF_SUCCESS)
        return hvx_len;
    else {
        DEBUG("Hello Demo data send err 0x", err_code);
    }
    return 0;
}

void hlo_ble_demo_init(const hlo_ble_demo_init_t *init)
{
    uint32_t   err_code;
    ble_uuid_t ble_uuid;
    uint8_t zeroes[20];

    memset(zeroes, 0xAC, sizeof(zeroes));

    // Init defaults
    _conn_handle = BLE_CONN_HANDLE_INVALID;
    _conn_handler       = &_default_conn_handler;
    _disconn_handler    = &_default_conn_handler;

    // Assign write handlers
    if (init->conn_handler)
        _conn_handler = init->conn_handler;

    if (init->disconn_handler)
        _disconn_handler = init->disconn_handler;

    // Add service
    ble_uuid.type = hello_type;
    ble_uuid.uuid = BLE_UUID_HELLO_DEMO_SVC;

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &_service_handle);
	APP_ERROR_CHECK(err_code);

    hlo_ble_char_notify_add(BLE_UUID_DATA_CHAR);
    hlo_ble_char_write_request_add(BLE_UUID_CONF_CHAR, init->mode_write_handler, 4);
    hlo_ble_char_write_request_add(BLE_UUID_CMD_CHAR, init->cmd_write_handler, 10);
    hlo_ble_char_read_add(BLE_UUID_GIT_DESCRIPTION_CHAR,
                          (uint8_t* const)GIT_DESCRIPTION,
                          strlen(GIT_DESCRIPTION));
}

uint16_t hlo_ble_demo_get_handle() {
    return hlo_ble_get_value_handle(BLE_UUID_CMD_CHAR);
}
