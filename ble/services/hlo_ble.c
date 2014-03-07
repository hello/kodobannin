// vi:noet:sw=4 ts=4

#include <app_error.h>
#include <string.h>

#include "hlo_ble.h"
#include "sha1.h"
#include "util.h"

struct hlo_ble_notify_context {
    /** This is an array of 15, because:

        1 element = hlo_ble_packet.header.data: carries 17 bytes; 239 bytes left.
        +
        13 elements = hlo_ble_packet.body.data: carries 19*13 (247) bytes; 0 bytes left.
        +
        1 element = hlo_ble_packet.footer: carries SHA-1 for data.
    */
    struct hlo_ble_packet packets[15];
    uint16_t packet_sizes[15];

    uint16_t characteristic_handle;

    uint8_t queued; //< number of packets queued to be sent
    uint8_t total; //< total number of packets to send

    hlo_ble_notify_callback callback;
};

static struct hlo_ble_notify_context _notify_context;


struct uuid_handler {
    uint16_t uuid;
    uint16_t value_handle;
    hlo_ble_write_handler handler;
};

uint8_t hello_type;

#define MAX_CHARACTERISTICS 9
static struct uuid_handler _uuid_handlers[MAX_CHARACTERISTICS];
static struct uuid_handler* _p_uuid_handler = _uuid_handlers;

static uint16_t _connection_handle = BLE_CONN_HANDLE_INVALID;

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

	ble_gatt_char_props_t notify_props = {};
	notify_props.notify = 1;

	_char_add(uuid, &notify_props, ignored, sizeof(ignored), NULL);
}

void
hlo_ble_char_indicate_add(uint16_t uuid)
{
	uint8_t ignored[BLE_GAP_DEVNAME_MAX_WR_LEN];

	ble_gatt_char_props_t indicate_props = {};
	indicate_props.indicate = 1;

	_char_add(uuid, &indicate_props, ignored, sizeof(ignored), NULL);
}

void
hlo_ble_char_write_request_add(uint16_t uuid, hlo_ble_write_handler write_handler, uint16_t max_buffer_size)
{
	uint8_t ignored[max_buffer_size];

	ble_gatt_char_props_t write_props = {};
	write_props.write = 1;

	_char_add(uuid, &write_props, ignored, sizeof(ignored), write_handler);
}

void
hlo_ble_char_write_command_add(uint16_t uuid, hlo_ble_write_handler write_handler, uint16_t max_buffer_size)
{
	uint8_t ignored[max_buffer_size];

	ble_gatt_char_props_t write_props = {};
	write_props.write_wo_resp = 1;

	_char_add(uuid, &write_props, ignored, sizeof(ignored), write_handler);
}

void
hlo_ble_char_read_add(uint16_t uuid, uint8_t* const value, uint16_t value_size)
{
	ble_gatt_char_props_t read_props = {};
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

static void
_dispatch_write(ble_evt_t *event) {
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

/** Returns true if we queued a packet for BLE notification; returns false if we did not queue a packet (for whatever reason, from no available buffers to other errors). */
static bool
_dispatch_notify()
{
    if(_notify_context.queued < _notify_context.total) {
        ble_gatts_hvx_params_t hvx_params = {
            .handle = _notify_context.characteristic_handle,
            .type = BLE_GATT_HVX_NOTIFICATION,
            .offset = 0,
            .p_len = &_notify_context.packet_sizes[_notify_context.queued],
            .p_data = _notify_context.packets[_notify_context.queued].bytes,
        };

#define PRINT_NOTIFY

#ifdef PRINT_NOTIFY
        PRINT_HEX(_notify_context.packets[_notify_context.queued].bytes, *hvx_params.p_len);
        PRINTS("\r\n");
#endif

        uint32_t err = sd_ble_gatts_hvx(_connection_handle, &hvx_params);

        switch(err) {
        case NRF_SUCCESS:
            _notify_context.queued++;
            return true;
        case BLE_ERROR_NO_TX_BUFFERS:
            break;
        default:
            DEBUG("sd_ble_gatts_hvx returned ", err);
        }
    } else if (_notify_context.total && _notify_context.queued == _notify_context.total) {
        hlo_ble_notify_callback callback = _notify_context.callback;

        _notify_context = (struct hlo_ble_notify_context) {};

        if(callback) {
            callback();
        }
    }

    return false;
}

static uint8_t
_packetize(void* src, uint16_t length)
{
    struct hlo_ble_packet* packet = _notify_context.packets;

    uint8_t* p = src;

    uint8_t sequence_number = 0;

    APP_ASSERT(length <= sizeof(packet->header.data) + sizeof(packet->body.data)*253);

    unsigned i = 0;

    // Write out header packet

    {
        packet->sequence_number = sequence_number++;

        packet->header.size = length;

        unsigned header_data_size = MIN(length, sizeof(packet->header.data));
        memcpy(packet->header.data, p, header_data_size);
        p += header_data_size;

        _notify_context.packet_sizes[i++] = header_data_size+sizeof(packet->header.size)+sizeof(packet->sequence_number);

        packet++;
    }

    // Write out body packets

    uint8_t* end = src+length;
    while(p < end) {
        packet->sequence_number = sequence_number++;

        size_t body_data_size = MIN(end-p, sizeof(packet->body.data));
        memcpy(packet->body.data, p, body_data_size);
        p += body_data_size;

        _notify_context.packet_sizes[i++] = body_data_size+sizeof(packet->sequence_number);

        packet++;
    }

    // Write out footer

    {
        packet->sequence_number = sequence_number++;

        uint8_t src_sha1[20];
        sha1_calc(src, length, src_sha1);
        memcpy(packet->footer.sha19, src_sha1, sizeof(packet->footer.sha19));

        _notify_context.packet_sizes[i++] = sizeof(packet->footer)+sizeof(packet->sequence_number);

        packet++;
    }

    uint8_t total = packet - _notify_context.packets;
    DEBUG("_packetize: total packets = ", total);

    return packet - _notify_context.packets;
}

void
hlo_ble_notify(uint16_t characteristic_uuid, uint8_t* data, uint16_t length, hlo_ble_notify_callback callback)
{
    _notify_context = (struct hlo_ble_notify_context) {
        .packet_sizes = { 0 },
        .characteristic_handle = hlo_ble_get_value_handle(characteristic_uuid),
        .queued = 0,
        .callback = callback,
    };

    _notify_context.total = _packetize(data, length);

    for(;;) {
        bool queued_packet = _dispatch_notify();

        if(queued_packet) {
            continue;
        } else {
            break;
        }
    }
}

uint16_t hlo_ble_get_connection_handle()
{
    return _connection_handle;
}

void hlo_ble_on_ble_evt(ble_evt_t* event)
{
    switch(event->header.evt_id) {
    case BLE_GAP_EVT_CONNECTED:
        _connection_handle = event->evt.gap_evt.conn_handle;
        DEBUG("Connect from MAC: ", event->evt.gap_evt.params.connected.peer_addr.addr);
        break;
    case BLE_GAP_EVT_DISCONNECTED:
        DEBUG("Disconnect from MAC: ", event->evt.gap_evt.params.disconnected.peer_addr.addr);
        DEBUG("Disconnect reason: ", event->evt.gap_evt.params.disconnected.reason);
        _connection_handle = BLE_CONN_HANDLE_INVALID;
        break;
    case BLE_GATTS_EVT_WRITE:
        _dispatch_write(event);
        break;
    case BLE_EVT_TX_COMPLETE:
        _dispatch_notify();
    default:
        break;
    }
}
