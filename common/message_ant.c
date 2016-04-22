#include "message_ant.h"
#include "util.h"
#include "hlo_queue.h"
#include <string.h>

static struct{
    MSG_Central_t * parent;
    MSG_Base_t base;
    const MSG_ANTHandler_t * user_handler;
    hlo_ant_packet_listener message_listener;
    hlo_queue_t * tx_queue;
    hlo_ant_role role;
    hlo_ant_device_t local_device;
}self;
static char * name = "ANT";

typedef struct{
    MSG_Data_t * msg;
}queue_message_t;

static MSG_Data_t INCREF * _AllocateAntPacket(MSG_ANT_PillDataType_t type, size_t payload_size);

static MSG_Status
_init(void){
    return SUCCESS;
}
static MSG_Status
_flush(void){
    return SUCCESS;
}
static void _handle_message(const hlo_ant_device_t * device, MSG_Data_t * message){
    self.user_handler->on_message(device, message);
}
static uint32_t
_queue_tx(MSG_Data_t * o){
    queue_message_t msg = (queue_message_t){
        .msg = o,
    };
    if( hlo_queue_empty_size(self.tx_queue) >= sizeof(msg) ){
        PRINTS("Queue\r\n");
        return hlo_queue_write(self.tx_queue, (unsigned char*)&msg, sizeof(msg));
    }else{
        return 1;
    }
    return 0;
}
static int32_t _try_send_ant_peripheral(MSG_Data_t * data, bool reliable){
    return hlo_ant_packet_send_message(&self.local_device, data, reliable);
}
static MSG_Status
_send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    switch(dst.submodule){
        default:
        case MSG_ANT_PING:
            break;
        case MSG_ANT_HANDLE_MESSAGE:
            if(data){
                MSG_ANT_Message_t * msg = (MSG_ANT_Message_t *)data->buf;
                _handle_message(&msg->device, msg->message);
                MSG_Base_ReleaseDataAtomic(msg->message);
            }
            break;
        case MSG_ANT_TRANSMIT:
            if(self.role == HLO_ANT_ROLE_PERIPHERAL){
                int32_t ret = _try_send_ant_peripheral(data, false);
                PRINTS("Sending:");
                PRINT_HEX(&ret, 2);
                PRINTS("\r\n");
                if( ret == -2 ){
                    MSG_Base_AcquireDataAtomic(data);
                    APP_ASSERT( (NRF_SUCCESS == _queue_tx(data)) );
                }
            }
            break;
        case MSG_ANT_TRANSMIT_RECEIVE:
            if(self.role == HLO_ANT_ROLE_PERIPHERAL){
                int32_t ret = _try_send_ant_peripheral(data, true);
                PRINTS("Sending:");
                PRINT_HEX(&ret, 2);
                PRINTS("\r\n");
                if( ret == -2 ){
                    MSG_Base_AcquireDataAtomic(data);
                    APP_ASSERT( (NRF_SUCCESS == _queue_tx(data)) );
                }
            }
            break;
    }
    return SUCCESS;
}

static MSG_Status
_destroy(void){
    return SUCCESS;
}
static void _on_message(const hlo_ant_device_t * device, MSG_Data_t * message){
    if(!message){
        return;  // Do not use one line if: https://medium.com/@jonathanabrams/single-line-if-statements-2565c62ff492
    }

    MSG_ANT_Message_t content = (MSG_ANT_Message_t){
        .device = *device,
        .message = message,
    };

    MSG_Data_t * parcel = MSG_Base_AllocateObjectAtomic(&content, sizeof(content));
    if(parcel){
        MSG_Base_AcquireDataAtomic(message);
        self.parent->dispatch( ADDR(ANT,0), ADDR(ANT,MSG_ANT_HANDLE_MESSAGE), parcel);
        MSG_Base_ReleaseDataAtomic(parcel);
    }
}
static MSG_Data_t * INCREF _on_connect(const hlo_ant_device_t * device){
    if( self.user_handler->on_connection ){
        return self.user_handler->on_connection(device);
    }
}

static uint32_t
_dequeue_tx(const hlo_ant_device_t * device){
    PRINTS("Dequeue\r\n");
    queue_message_t out;
    uint32_t ret = hlo_queue_read(self.tx_queue, (unsigned char *)&out, sizeof(out));
    if(ret == NRF_SUCCESS){
        MSG_Base_ReleaseDataAtomic(out.msg);
        self.parent->dispatch( ADDR(ANT,0), ADDR(ANT, MSG_ANT_TRANSMIT), out.msg);
    }else{
        ret = hlo_ant_disconnect(device);
    }
    return ret;
}
static void _on_message_sent(const hlo_ant_device_t * device, MSG_Data_t * message){
    //get next queued tx message
    PRINTS("message sent \r\n");
    if(self.role == HLO_ANT_ROLE_PERIPHERAL){
        APP_OK(_dequeue_tx(device));
    }
}
static void _on_message_failed(const hlo_ant_device_t * device, MSG_Data_t * message){
    PRINTS("message failed \r\n");
    if(self.role == HLO_ANT_ROLE_PERIPHERAL){
        static int retry;
        if(retry++ < 3){
            PRINTS("retry...");
            self.parent->dispatch(ADDR(ANT,0), ADDR(ANT,MSG_ANT_TRANSMIT), message);
        }else{
            PRINTS("drop...");
            retry = 0;
            _dequeue_tx(device);
        }
    }
}


static MSG_Data_t INCREF * _AllocateAntPacket(MSG_ANT_PillDataType_t type, size_t payload_size){
    MSG_Data_t* data_page = MSG_Base_AllocateDataAtomic(sizeof(MSG_ANT_PillData_t) + payload_size);
    if(data_page){
        memset(&data_page->buf, 0, data_page->len);

        MSG_ANT_PillData_t *ant_data =(MSG_ANT_PillData_t*) &data_page->buf;
        ant_data->version = ANT_PROTOCOL_VER;
        ant_data->type = type;
        ant_data->UUID = GET_UUID_64();
        ant_data->payload_len = payload_size;
    }
    return data_page;
}
typedef struct{
    uint64_t nonce;
    uint8_t payload[0];
}__attribute__((packed)) MSG_ANT_EncryptedData20_t;
MSG_Data_t * INCREF AllocateEncryptedAntPayload(MSG_ANT_PillDataType_t type, void * payload, size_t len){
    MSG_Data_t* data_page = _AllocateAntPacket(type ,sizeof(uint64_t) + len);
    if( data_page ){
        //ant data comes from the data page (allocated above, and freed at the end of this function)
        MSG_ANT_PillData_t *ant_data =(MSG_ANT_PillData_t*)data_page->buf;
        //motion_data is a pointer to the blob of data that antdata->payload points to
        //the goal is to fill out the motion_data pointer
        MSG_ANT_EncryptedData20_t *edata = (MSG_ANT_EncryptedData20_t *)ant_data->payload;

        //now set the nonce
        uint8_t pool_size = 0;
        uint8_t nonce[8] = {0};
        if(NRF_SUCCESS == sd_rand_application_bytes_available_get(&pool_size)){
            sd_rand_application_vector_get(nonce, (pool_size > sizeof(nonce) ? sizeof(nonce) : pool_size));
        }
        memcpy(&edata->nonce, nonce, sizeof(nonce));

        //now encrypt
        memcpy(edata->payload, payload, len);
        aes128_ctr_encrypt_inplace(edata->payload, len, get_aes128_key(), nonce);
    }
    return data_page;
}
MSG_Data_t * INCREF AllocateAntPayload(MSG_ANT_PillDataType_t type, void * payload, size_t len){
    MSG_Data_t* data_page = _AllocateAntPacket(type ,len);
    if( data_page ){
        MSG_ANT_PillData_t *ant_data =(MSG_ANT_PillData_t*) &data_page->buf;
        memcpy(ant_data->payload, payload, len);
    }
    return data_page;
}

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent, const MSG_ANTHandler_t * handler,hlo_ant_role role, uint8_t device_type){
    self.parent = parent;
    self.user_handler = handler;
    self.tx_queue = hlo_queue_init(64);
    APP_ASSERT(self.tx_queue);
    {//plug in message
        self.base.init =  _init;
        self.base.flush = _flush;
        self.base.send = _send;
        self.base.destroy = _destroy;
        self.base.type = ANT;
        self.base.typestr = name;
    }

    {//driver side init
        self.message_listener.on_connect = _on_connect;
        self.message_listener.on_message = _on_message;
        self.message_listener.on_message_sent = _on_message_sent;
        self.message_listener.on_message_failed = _on_message_failed;
        APP_OK(hlo_ant_init(role, hlo_ant_packet_init(&self.message_listener)));
        self.role = role;
    }
    switch(role){
        default:
        case HLO_ANT_ROLE_CENTRAL:
            break;
        case HLO_ANT_ROLE_PERIPHERAL:
            self.local_device = (hlo_ant_device_t){
                .device_number = GET_UUID_16(),
                .device_type = device_type,
                .transmit_type = 1,
            };
            break;
    }
    return &self.base;
}
