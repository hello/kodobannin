/* Copyright (c) 2013 Hello Inc. All Rights Reserved. */

#include "ble_hello_demo.h"

#include <stdlib.h>
#include <string.h>
#include <app_error.h>
#include <ble_gatts.h>
#include <nordic_common.h>
#include <ble_srv_common.h>
#include <app_util.h>
#include <util.h>
#include <ble.h>

uint8_t hello_type;
static uint16_t                 service_handle;
static uint16_t                 conn_handle;
static ble_hello_demo_write_handler    data_write_handler;
static ble_hello_demo_write_handler    mode_write_handler;
static ble_hello_demo_write_handler    cmd_write_handler;
static ble_hello_demo_connect_handler  conn_handler;
static ble_hello_demo_connect_handler  disconn_handler;
static volatile uint32_t buffers_available = 0;

typedef uint32_t (*notify_data_send_cb)(const uint8_t *data, const uint16_t len);
static notify_data_send_cb data_cb = NULL;
static uint8_t *data_cb_data;
static uint16_t data_cb_len;

static void
default_write_handler(ble_gatts_evt_write_t *event) {
    DEBUG("Default write handler called for 0x", event->handle);
}

static void
default_conn_handler(void) {
    PRINTS("Default conn handler called");
}

typedef void(*char_write_handler_t)(ble_gatts_evt_write_t*);

typedef struct {
	uint16_t uuid;
	ble_gatts_char_handles_t handles;
	char_write_handler_t handler;
} uuid_handler_t;

#define MAX_CHARACTERISTICS 4
static uuid_handler_t _uuid_handlers[MAX_CHARACTERISTICS];
static uuid_handler_t* _p_uuid_handler = _uuid_handlers;

uint16_t _value_handle(const uint16_t uuid) {
	uuid_handler_t* p;
	for(p = _uuid_handlers; p < _uuid_handlers+MAX_CHARACTERISTICS; p++) {
		if(p->uuid == uuid) {
			return p->handles.value_handle;
		}
	}

	APP_ASSERT(0);
}

uint16_t ble_hello_demo_get_handle() {
	return _value_handle(BLE_UUID_CMD_CHAR);
}

static void
dispatch_write(ble_evt_t *event) {
    ble_gatts_evt_write_t* const write_evt = &event->evt.gatts_evt.params.write;
    const uint16_t handle = write_evt->handle;

	uuid_handler_t* p;
	for(p = _uuid_handlers; p < _uuid_handlers+MAX_CHARACTERISTICS; p++) {
		if(p->handles.value_handle == handle) {
			p->handler(write_evt);
			return;
		}
	}

	DEBUG("Couldn't locate write handler for handle: ", handle);
}

void
ble_hello_demo_on_ble_evt(ble_evt_t *event)
{
    DEBUG("event id: 0x", event->header.evt_id)
    switch (event->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            conn_handle = event->evt.gap_evt.conn_handle;
            conn_handler();
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            conn_handle = BLE_CONN_HANDLE_INVALID;
            disconn_handler();
            break;

        case BLE_GATTS_EVT_WRITE:
            dispatch_write(event);
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
ble_hello_demo_data_send_blocking(const uint8_t *data, const uint16_t len) {
    uint32_t err;
    int32_t hvx_len;
    uint16_t pkt_len;
    ble_gatts_hvx_params_t hvx_params;
    uint8_t *ptr;

    data_cb = NULL;

    //todo: return verbose errors instead of 0
    if (conn_handle == BLE_CONN_HANDLE_INVALID)
        return 0;

    hvx_len = len;
    ptr = (uint8_t *)data;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle   = _value_handle(BLE_UUID_DATA_CHAR);
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
        err = sd_ble_gatts_hvx(conn_handle, &hvx_params);
        if (err == NRF_SUCCESS) {
            hvx_len -= pkt_len;
            ptr += pkt_len;
            //DEBUG("SENT PKT ", hvx_len);
        } else if (err == BLE_ERROR_NO_TX_BUFFERS) {
            //DEBUG("NEED BUF, ", hvx_len);
            // so we cant wait here because we're blocking the softdevice from actually sending
            // right now, we return an in progress indicator, and request that we be called back
            // with our in-progress data ptr and bytes
            data_cb = &ble_hello_demo_data_send_blocking;
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
ble_hello_demo_data_send(const uint8_t *data, const uint16_t len) {
    uint32_t err_code;
    uint16_t hvx_len;
    ble_gatts_hvx_params_t hvx_params;

    //todo: return verbose errors instead of 0
    if (conn_handle == BLE_CONN_HANDLE_INVALID)
        return 0;

    hvx_len = len;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle   = _value_handle(BLE_UUID_DATA_CHAR);
    hvx_params.type     = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset   = 0;
    hvx_params.p_len    = &hvx_len;
    hvx_params.p_data   = (uint8_t *)data;

    err_code = sd_ble_gatts_hvx(conn_handle, &hvx_params);
    if (err_code == NRF_SUCCESS)
        return hvx_len;
    else {
        DEBUG("Hello Demo data send err 0x", err_code);
    }
    return 0;
}

static uint32_t
_char_add(const uint16_t uuid,
		   ble_gatt_char_props_t* const props,
		   uint8_t* const p_value,
		   const uint16_t max_value_size,
		   char_write_handler_t write_handler)
{
	APP_ASSERT(_p_uuid_handler >= _uuid_handlers && _p_uuid_handler < _uuid_handlers+MAX_CHARACTERISTICS);

    ble_uuid_t ble_uuid;
    BLE_UUID_BLE_ASSIGN(ble_uuid, uuid);

    ble_gatts_char_md_t char_md;
	memset(&char_md, 0, sizeof(char_md));
    char_md.char_props = *props;

    ble_gatts_attr_md_t attr_md;
    memset(&attr_md, 0, sizeof(attr_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen = 1;

    ble_gatts_attr_t attr_char_value;
    memset(&attr_char_value, 0, sizeof(attr_char_value));
    attr_char_value.p_uuid = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len = max_value_size;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len = max_value_size;
    attr_char_value.p_value = p_value;

	ble_gatts_char_handles_t handles;
    uint32_t err_code = sd_ble_gatts_characteristic_add(service_handle,
														&char_md,
														&attr_char_value,
														&handles);
	if(err_code)
		return err_code;

	*_p_uuid_handler++ = (uuid_handler_t) {
		.uuid = uuid,
		.handles = handles,
		.handler = write_handler
	};

	return err_code;
}

uint32_t ble_hello_demo_init(const ble_hello_demo_init_t *init) {
    uint32_t   err_code;
    ble_uuid_t ble_uuid;
    const ble_uuid128_t hello_uuid = {.uuid128 = BLE_UUID_HELLO_BASE};
    uint8_t zeroes[20];

    memset(zeroes, 0xAC, sizeof(zeroes));

    // Init defaults
    conn_handle = BLE_CONN_HANDLE_INVALID;
    data_write_handler = &default_write_handler;
    mode_write_handler = &default_write_handler;
    cmd_write_handler  = &default_write_handler;
    conn_handler       = &default_conn_handler;
    disconn_handler    = &default_conn_handler;

    // Assign write handlers
    if (init->data_write_handler)
        data_write_handler = init->data_write_handler;

    if (init->mode_write_handler)
        mode_write_handler = init->mode_write_handler;

    if (init->cmd_write_handler)
        cmd_write_handler = init->cmd_write_handler;

    if (init->conn_handler)
        conn_handler = init->conn_handler;

    if (init->disconn_handler)
        disconn_handler = init->disconn_handler;

    // Add Hello's Base UUID
    err_code = sd_ble_uuid_vs_add(&hello_uuid, &hello_type);
    if (err_code != NRF_SUCCESS)
        return err_code;

    // Add service
    ble_uuid.type = hello_type;
    ble_uuid.uuid = BLE_UUID_HELLO_DEMO_SVC;

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &service_handle);
    if (err_code != NRF_SUCCESS)
        return err_code;

	ble_gatt_char_props_t notify_props;
	memset(&notify_props, 0, sizeof(notify_props));
	notify_props.notify = 1;

	ble_gatt_char_props_t rw_props;
	memset(&rw_props, 0, sizeof(rw_props));
	rw_props.read = 1;
	rw_props.write = 1;

    // Add characteristics
    err_code = _char_add(BLE_UUID_DATA_CHAR,
						 &notify_props,
						 zeroes,
						 BLE_GAP_DEVNAME_MAX_WR_LEN,
						 data_write_handler);
    if (err_code != NRF_SUCCESS)
        return err_code;

    err_code = _char_add(BLE_UUID_CONF_CHAR,
						 &rw_props,
						 zeroes,
						 4,
						 mode_write_handler);
    if (err_code != NRF_SUCCESS)
        return err_code;

    err_code = _char_add(BLE_UUID_CMD_CHAR,
						 &rw_props,
						 zeroes,
						 10,
						 cmd_write_handler);
    if (err_code != NRF_SUCCESS)
        return err_code;

    return NRF_SUCCESS;
}
