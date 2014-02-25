// vi:noet:sw=4 ts=4

#include <app_error.h>
#include <string.h>

#include "hlo_ble.h"
#include "util.h"

struct uuid_handler {
    uint16_t uuid;
    uint16_t value_handle;
    hlo_ble_write_handler handler;
};

uint8_t hello_type;

#define MAX_CHARACTERISTICS 9
static struct uuid_handler _uuid_handlers[MAX_CHARACTERISTICS];
static struct uuid_handler* _p_uuid_handler = _uuid_handlers;

static void
_char_add(const uint16_t uuid,
		  ble_gatt_char_props_t* const props,
		  uint8_t* const p_value,
		  const uint16_t max_value_size,
		  hlo_ble_write_handler write_handler)
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
hlo_ble_char_notify_add(uint16_t uuid)
{
	uint8_t ignored[BLE_GAP_DEVNAME_MAX_WR_LEN];

	ble_gatt_char_props_t notify_props;
	memset(&notify_props, 0, sizeof(notify_props));
	notify_props.notify = 1;

	_char_add(uuid, &notify_props, ignored, sizeof(ignored), NULL);
}

void
hlo_ble_char_indicate_add(uint16_t uuid)
{
	uint8_t ignored[BLE_GAP_DEVNAME_MAX_WR_LEN];

	ble_gatt_char_props_t indicate_props;
	memset(&indicate_props, 0, sizeof(indicate_props));
	indicate_props.indicate = 1;

	_char_add(uuid, &indicate_props, ignored, sizeof(ignored), NULL);
}

void
hlo_ble_char_write_request_add(uint16_t uuid, hlo_ble_write_handler write_handler, uint16_t max_buffer_size)
{
	uint8_t ignored[max_buffer_size];

	ble_gatt_char_props_t write_props;
	memset(&write_props, 0, sizeof(write_props));
	write_props.write = 1;

	_char_add(uuid, &write_props, ignored, sizeof(ignored), write_handler);
}

void
hlo_ble_char_write_command_add(uint16_t uuid, hlo_ble_write_handler write_handler, uint16_t max_buffer_size)
{
	uint8_t ignored[max_buffer_size];

	ble_gatt_char_props_t write_props;
	memset(&write_props, 0, sizeof(write_props));
	write_props.write_wo_resp = 1;

	_char_add(uuid, &write_props, ignored, sizeof(ignored), write_handler);
}

void
hlo_ble_char_read_add(uint16_t uuid, uint8_t* const value, uint16_t value_size)
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

uint16_t
hlo_ble_get_value_handle(const uint16_t uuid)
{
    struct uuid_handler* p;
    for(p = _uuid_handlers; p < _uuid_handlers+MAX_CHARACTERISTICS; p++) {
        if(p->uuid == uuid) {
            return p->value_handle;
        }
    }

    APP_ASSERT(0);
}

void
hlo_ble_dispatch_write(ble_evt_t *event) {
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
