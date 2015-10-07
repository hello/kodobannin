// vi:noet:sw=4 ts=4

#include <app_error.h>
#include <string.h>

#include "morpheus_gatt.h"
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
    //struct hlo_ble_packet packets[15];
    //uint16_t packet_sizes[15];

    uint16_t characteristic_handle;

    uint8_t seq; //< number of packets queued to be sent
    uint8_t total; //< total number of packets to send

	uint8_t *orig; //origin of buffer to be sent
	uint8_t *current; //current pointer of the buffer to be sent
	uint8_t *end;//end of the buffer to be sent
	uint8_t last_len; // You have it!

    struct hlo_ble_operation_callbacks callback_info;
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

static volatile uint16_t _connection_handle = BLE_CONN_HANDLE_INVALID;

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
	ble_gatts_attr_md_t cccd_md;

	memset(&char_md, 0, sizeof(char_md));
	memset(&cccd_md, 0, sizeof(cccd_md));

	char_md.char_props = *props;
	char_md.p_cccd_md = &cccd_md;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;

	ble_gatts_attr_md_t attr_md;
	memset(&attr_md, 0, sizeof(attr_md));
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

	//BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attr_md.read_perm);
	//BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attr_md.write_perm);

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
	uint8_t ignored[BLE_GAP_DEVNAME_MAX_LEN];

	ble_gatt_char_props_t notify_props = {};
	notify_props.notify = 1;

	_char_add(uuid, &notify_props, ignored, sizeof(ignored), NULL);
}

void
hlo_ble_char_indicate_add(uint16_t uuid)
{
	uint8_t ignored[BLE_GAP_DEVNAME_MAX_LEN];

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

    // OS X Mavericks (10.9) has a bug where a Write Command
    // (i.e. write without confirmation) to a characteristic will not
    // work, unless that characteristic also supports Write Requests
    // (i.e. write with confirmation). As such, we turn on suport for
    // Write Requests here too.
    write_props.write = 1;

	_char_add(uuid, &write_props, ignored, sizeof(ignored), write_handler);
}

void
hlo_ble_char_read_add(uint16_t uuid, uint8_t* const value, uint16_t value_size)
{
	ble_gatt_char_props_t read_props = {};
	read_props.read = 1;

	_char_add(uuid, &read_props, value, value_size, NULL);
}
void get_random(int num_rand_bytes, uint8_t *rand_data);

void hlo_ble_init()
{
    const ble_uuid128_t hello_uuid = {.uuid128 = BLE_UUID_HELLO_BASE};

    ble_gap_addr_t gap_addr;
    gap_addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
    get_random(BLE_GAP_ADDR_LEN, gap_addr.addr);
    gap_addr.addr[5] |= 0xc0; // 2 MSBit must be '11' for RANDOM_STATIC address, see v4.0, Vol 3, Part C, chapter 10.8
    APP_OK( sd_ble_gap_address_set(&gap_addr));
    
	APP_OK(sd_ble_uuid_vs_add(&hello_uuid, &hello_type));
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

    PRINTS("hlo_ble_get_value_handle error\r\n");

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

static bool _dispatch_packet(struct hlo_ble_packet * t){
    if(_connection_handle == BLE_CONN_HANDLE_INVALID)
    {
        if(_notify_context.callback_info.on_failed)
        {
            _notify_context.callback_info.on_failed(_notify_context.callback_info.callback_data);
        }
        return false;
    }

	uint16_t mlen = (t->sequence_number == _notify_context.total - 1 ? _notify_context.last_len : 20);
	ble_gatts_hvx_params_t hvx_params = {
		.handle = _notify_context.characteristic_handle,
		.type = BLE_GATT_HVX_NOTIFICATION,
		.offset = 0,
		.p_len = &mlen,
		.p_data = (uint8_t*)t,
	};
	PRINTS("Sending BLE Packet:");
	PRINT_HEX(t, mlen);
	PRINTS("\r\n");
	
	uint32_t err = sd_ble_gatts_hvx(_connection_handle, &hvx_params);
	switch(err){
		case NRF_SUCCESS:
			PRINTS("BLE Packet sent\r\n");
			return true;
			break;
		default:
		case BLE_ERROR_NO_TX_BUFFERS:
			PRINTS("Send notification failed: ");
			PRINT_HEX(&err, 4);
			PRINTS("\r\n");
			if(_notify_context.callback_info.on_failed)
			{
				_notify_context.callback_info.on_failed(_notify_context.callback_info.callback_data);
			}
			return false;
			break;
	}
}

static uint16_t 
_rem(const uint8_t * src, const uint8_t * dest){
	if(dest >= src){
		return (dest-src)+1;
	}else{
		return 0;
	}
}

//this function is not reentrant!!111
static struct hlo_ble_packet * _make_packet(void){
	static struct hlo_ble_packet packet;
	if(_notify_context.current > _notify_context.end){
		return NULL;
	}
	uint16_t advance;
	uint16_t rem = _rem(_notify_context.orig, _notify_context.end);
	packet.sequence_number = _notify_context.seq++;
	if(_notify_context.current == _notify_context.orig){
		//header
		advance = (rem>18)?18:rem;
		packet.header.packet_count = _notify_context.total;
		memcpy(packet.header.data, _notify_context.current, advance);
	}else{
		//everything
		advance = (rem>19)?19:rem;
		memcpy(packet.body.data, _notify_context.current,advance);
	}
	//PRINT_HEX(&packet, 20);
	PRINTS("\r\n");
	_notify_context.current += advance;
	return &packet;
}

static inline uint8_t _calculate_total(uint16_t length){
	//assume header and no footer
	return (length + 1) / 19 + ((length + 1) % 19 > 0 ? 1 : 0);
}

static inline uint8_t _last_packet_len(uint16_t length){

	uint8_t remain = (length - 18) % 19;
	return length <= 18 ? (length + 2) : (remain == 0 ? 20 : remain + 1);
}


void hlo_ble_notify(uint16_t characteristic_uuid, uint8_t* data, uint16_t length, const struct hlo_ble_operation_callbacks* callback_info)
{
	if(length == 0)
    {
        return;
    }

    if(_connection_handle == BLE_CONN_HANDLE_INVALID)
    {
        if(callback_info->on_failed)
        {
            callback_info->on_failed(callback_info->callback_data);
        }
        return;
    }

    _notify_context = (struct hlo_ble_notify_context) {
        .characteristic_handle = hlo_ble_get_value_handle(characteristic_uuid),
        .seq = 0,
        .callback_info = callback_info == NULL ? (struct hlo_ble_operation_callbacks){} : (*callback_info),
		.orig = data,
		.current = data,
		.end = (data + length - 1),
		.last_len = _last_packet_len(length),
		.total = _calculate_total(length)//TODO actual total
    };

	struct hlo_ble_packet * packet = _make_packet();
	if(packet){
		_dispatch_packet(packet);
	}


}

bool hlo_ble_is_connected()
{
    return hlo_ble_get_connection_handle() != BLE_CONN_HANDLE_INVALID;
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
        PRINTS("Connect from MAC: ");
        PRINT_HEX(&event->evt.gap_evt.params.connected.peer_addr.addr, sizeof(event->evt.gap_evt.params.connected.peer_addr.addr));
        PRINTS("\r\n");
        break;
    case BLE_GAP_EVT_DISCONNECTED:
        PRINTS("Disconnected: ");
        PRINT_HEX(&event->evt.gap_evt.params.disconnected.reason, sizeof(event->evt.gap_evt.params.disconnected.reason));
        PRINTS("\r\n");
        _connection_handle = BLE_CONN_HANDLE_INVALID;
        break;
    case BLE_GATTS_EVT_WRITE:
        _dispatch_write(event);
        break;
    case BLE_EVT_TX_COMPLETE:
		{
			if(_notify_context.seq == _notify_context.total){
				if(_notify_context.callback_info.on_succeed){
					_notify_context.callback_info.on_succeed(_notify_context.orig, _notify_context.callback_info.callback_data);
				}
			}else{
				struct hlo_ble_packet * packet = _make_packet();
				if(packet){
					_dispatch_packet(packet);
				}
			}
		}
        //_dispatch_queue_packet();
        break;
	case BLE_GAP_EVT_CONN_PARAM_UPDATE:
		{
			uint32_t sl = event->evt.gap_evt.params.conn_param_update.conn_params.slave_latency * 1.25;
			uint32_t interval = event->evt.gap_evt.params.conn_param_update.conn_params.max_conn_interval * 1.25;
			uint32_t sto  = event->evt.gap_evt.params.conn_param_update.conn_params.conn_sup_timeout * 10;
			PRINTS("Param Update in ms (sl, int, sto)\r\n");
			PRINT_DEC(&sl);
			PRINTS(" ");
			PRINT_DEC(&interval);
			PRINTS(" ");
			PRINT_DEC(&sto);
			PRINTS("\r\n");
		}
		break;
	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		{
			uint32_t ret;
			ret = sd_ble_gatts_sys_attr_set(_connection_handle,NULL, 0);
			DEBUG("Attribute missing: ", ret);
		}
		break;
    default:
        PRINTS("Unknown BLE event: ");
        PRINT_HEX(&event->header.evt_id, sizeof(event->header.evt_id));
        PRINTS("\r\n");
        break;
    }
}
