// vi:sw=4:ts=4

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

uint8_t hello_type;
static uint16_t                 _service_handle;
static uint16_t                 _conn_handle;
static hlo_ble_demo_connect_handler  _conn_handler;
static hlo_ble_demo_connect_handler  _disconn_handler;

static uint16_t _alpha0_service_handle;

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
    PRINTS("Default conn handler called\r\n");
}

typedef void(*char_write_handler_t)(ble_gatts_evt_write_t*);

struct uuid_handler {
	uint16_t uuid;
	uint16_t value_handle;
	char_write_handler_t handler;
};

#define MAX_CHARACTERISTICS 9
static struct uuid_handler _uuid_handlers[MAX_CHARACTERISTICS];
static struct uuid_handler* _p_uuid_handler = _uuid_handlers;

uint16_t _value_handle(const uint16_t uuid) {
	struct uuid_handler* p;
	for(p = _uuid_handlers; p < _uuid_handlers+MAX_CHARACTERISTICS; p++) {
		if(p->uuid == uuid) {
			return p->value_handle;
		}
	}

	APP_ASSERT(0);
}

uint16_t hlo_ble_demo_get_handle() {
	return _value_handle(BLE_UUID_CMD_CHAR);
}

static void
dispatch_write(ble_evt_t *event) {
    ble_gatts_evt_write_t* const write_evt = &event->evt.gatts_evt.params.write;
    const uint16_t handle = write_evt->handle;

	struct uuid_handler* p;
	for(p = _uuid_handlers; p < _uuid_handlers+MAX_CHARACTERISTICS; p++) {
		if(p->value_handle == handle) {
			p->handler(write_evt);
			return;
		}
	}

	DEBUG("Couldn't locate write handler for handle: ", handle);
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

    hvx_params.handle   = _value_handle(BLE_UUID_DATA_CHAR);
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

static void
_char_add(const uint16_t uuid,
		   ble_gatt_char_props_t* const props,
		   uint8_t* const p_value,
		   const uint16_t max_value_size,
		   char_write_handler_t write_handler)
{
	APP_ASSERT(_p_uuid_handler >= _uuid_handlers
			   && _p_uuid_handler < _uuid_handlers+MAX_CHARACTERISTICS);

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
    attr_char_value.p_value = (uint8_t* const)p_value;

	ble_gatts_char_handles_t handles;
    uint32_t err_code = sd_ble_gatts_characteristic_add(BLE_GATT_HANDLE_INVALID,
														&char_md,
														&attr_char_value,
														&handles);
	APP_ERROR_CHECK(err_code);

	*_p_uuid_handler++ = (struct uuid_handler) {
		.uuid = uuid,
		.value_handle = handles.value_handle,
		.handler = write_handler
	};
}

void
ble_char_notify_add(uint16_t uuid)
{
	uint8_t ignored[BLE_GAP_DEVNAME_MAX_WR_LEN];

	ble_gatt_char_props_t notify_props;
	memset(&notify_props, 0, sizeof(notify_props));
	notify_props.notify = 1;

	_char_add(uuid, &notify_props, ignored, sizeof(ignored), NULL);
}

void
ble_char_indicate_add(uint16_t uuid)
{
    uint8_t ignored[BLE_GAP_DEVNAME_MAX_WR_LEN];

    ble_gatt_char_props_t indicate_props;
    memset(&indicate_props, 0, sizeof(indicate_props));
    indicate_props.indicate = 1;

    _char_add(uuid, &indicate_props, ignored, sizeof(ignored), NULL);
}

void
ble_char_write_request_add(uint16_t uuid, hlo_ble_demo_write_handler write_handler, uint16_t max_buffer_size)
{
	uint8_t ignored[max_buffer_size];

	ble_gatt_char_props_t write_props;
	memset(&write_props, 0, sizeof(write_props));
	write_props.write = 1;

	_char_add(uuid, &write_props, ignored, sizeof(ignored), write_handler);
}

void
ble_char_write_command_add(uint16_t uuid, hlo_ble_demo_write_handler write_handler, uint16_t max_buffer_size)
{
    uint8_t ignored[max_buffer_size];

    ble_gatt_char_props_t write_props;
    memset(&write_props, 0, sizeof(write_props));
    write_props.write_wo_resp = 1;

    _char_add(uuid, &write_props, ignored, sizeof(ignored), write_handler);
}

void
ble_char_read_add(uint16_t uuid, uint8_t* const value, uint16_t value_size)
{
	ble_gatt_char_props_t read_props;
	memset(&read_props, 0, sizeof(read_props));
	read_props.read = 1;

	_char_add(uuid, &read_props, value, value_size, NULL);
}

void hlo_ble_init()
{
	const ble_uuid128_t hello_uuid = {.uuid128 = BLE_UUID_HELLO_BASE};

	uint32_t err_code = sd_ble_uuid_vs_add(&hello_uuid, &hello_type);
	APP_ERROR_CHECK(err_code);
}

void hlo_ble_demo_init(const hlo_ble_demo_init_t *init)
{
    uint32_t   err_code;
    ble_uuid_t ble_uuid;
    uint8_t zeroes[20];

    memset(zeroes, 0xAC, sizeof(zeroes));

    // Init defaults
    _conn_handle = BLE_CONN_HANDLE_INVALID;
    _conn_handler       = &default_conn_handler;
    _disconn_handler    = &default_conn_handler;

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
}

void hlo_ble_alpha0_init()
{
    ble_uuid_t ble_uuid = {
        .type = hello_type,
        .uuid = BLE_UUID_HELLO_ALPHA0_SVC,
    };

    uint32_t err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &_alpha0_service_handle);
    APP_ERROR_CHECK(err_code);
}
