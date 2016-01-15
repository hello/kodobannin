#include "message_ant.h"
#include "util.h"
#include "hlo_queue.h"
#include <string.h>

static struct{
    MSG_Central_t * parent;
    MSG_Base_t base;
    const MSG_ANTHandler_t * user_handler;
    hlo_ant_packet_listener message_listener;
    hlo_ant_device_t paired_devices[DEFAULT_ANT_BOND_COUNT];//TODO macro this in later
    hlo_queue_t * tx_queue;
}self;
static char * name = "ANT";

typedef struct{
    MSG_Data_t * msg;
    MSG_Address_t address;
}queue_message_t;

static MSG_Data_t INCREF * _AllocateAntPacket(MSG_ANT_PillDataType_t type, size_t payload_size);
//returns -1 if not found, otherwise return index
static int
_find_paired(const hlo_ant_device_t * device){
    int ret;
    for(ret = 0; ret < DEFAULT_ANT_BOND_COUNT; ret++){
        if(self.paired_devices[ret].device_number == device->device_number){
            return ret;
        }
    }
    return -1;
}
static int
_find_empty(void){
    int ret;
    for(ret = 0; ret < DEFAULT_ANT_BOND_COUNT; ret++){
        if(self.paired_devices[ret].device_number == 0){
            return ret;
        }
    }
    return -1;
}
static MSG_Status
_init(void){
    return SUCCESS;
}
static MSG_Status
_flush(void){
    return SUCCESS;
}
static void _handle_message(const hlo_ant_device_t * device, MSG_Data_t * message){
    MSG_Address_t default_src = {ANT, 0};
    self.user_handler->on_message(device, default_src, message);
}
static uint32_t
_queue_tx(MSG_Data_t * o, MSG_Address_t address){
    queue_message_t msg = {
        .msg = o,
        .address = address,
    };
    if( hlo_queue_empty_size(self.tx_queue) >= sizeof(msg) ){
        PRINTS("Queue\r\n");
        return hlo_queue_write(self.tx_queue, (unsigned char*)&msg, sizeof(msg));
    }else{
        return 1;
    }
    return 0;
}
static int32_t _try_send_ant(MSG_Data_t * data, MSG_Address_t dst){
    int idx = dst.submodule - MSG_ANT_CONNECTION_BASE;
    if(self.paired_devices[idx].device_number){
        return hlo_ant_packet_send_message(&self.paired_devices[idx], data);
    }else{
        return -1;
    }
    return -3;
}
static MSG_Status
_send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    if(dst.submodule >= (uint8_t) MSG_ANT_CONNECTION_BASE &&
            dst.submodule < (uint8_t)(MSG_ANT_CONNECTION_BASE + DEFAULT_ANT_BOND_COUNT)){
        int32_t ret = _try_send_ant(data, dst);
        PRINTS("Sending:");
        PRINT_HEX(&ret, 2);
        PRINTS("\r\n");
        if( ret == -2 ){
            MSG_Base_AcquireDataAtomic(data);
            APP_ASSERT( (NRF_SUCCESS == _queue_tx(data, dst)) );
        }
    }else{
        switch(dst.submodule){
            default:
            case MSG_ANT_PING:
                break;
            case MSG_ANT_SET_ROLE:
                if(data){
                    hlo_ant_role * role = (hlo_ant_role*)data->buf;
                    if(0 == hlo_ant_init(*role, hlo_ant_packet_init(&self.message_listener))){
                    }
                }
                break;
            case MSG_ANT_REMOVE_DEVICE:
                if(data){
                    hlo_ant_device_t * device = (hlo_ant_device_t*)data->buf;
                    int i = _find_paired(device);
                    if(i >= 0){
                        hlo_ant_device_t * dev = &self.paired_devices[(uint8_t)i];
                        dev->device_number = 0;
                        if(self.user_handler && self.user_handler->on_status_update){
                            self.user_handler->on_status_update(device, ANT_STATUS_DISCONNECTED);
                        }
                    }
                }
                break;
            case MSG_ANT_ADD_DEVICE:
                if(data){
                    hlo_ant_device_t * device = (hlo_ant_device_t*)data->buf;
                    int i = _find_empty();
                    if(i >= 0){
                        self.paired_devices[(uint8_t)i] = *device;
                        if(self.user_handler && self.user_handler->on_status_update){
                            self.user_handler->on_status_update(device, ANT_STATUS_CONNECTED);
                        }
                    }
                }
                break;
            case MSG_ANT_HANDLE_MESSAGE:
                if(data){
                    MSG_ANT_Message_t * msg = (MSG_ANT_Message_t *)data->buf;
                    _handle_message(&msg->device, msg->message);
                    MSG_Base_ReleaseDataAtomic(msg->message);
                }
                break;
        }
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

static uint32_t
_dequeue_tx(queue_message_t * out_msg){
    PRINTS("Dequeue\r\n");
    uint32_t ret = hlo_queue_read(self.tx_queue, (unsigned char *)out_msg, sizeof(*out_msg));
    return ret;
}
static void _on_message_sent(const hlo_ant_device_t * device, MSG_Data_t * message){
    //get next queued tx message
    queue_message_t out;
    uint32_t ret = _dequeue_tx(&out);
    if( ret == NRF_SUCCESS ){
        MSG_Base_ReleaseDataAtomic(out.msg);
        self.parent->dispatch((MSG_Address_t){ANT,0}, out.address, out.msg);
    }else{
        int ret = hlo_ant_disconnect(device);
        PRINTS("All Message Sent!\r\n");
        if(ret < 0){
            PRINTS("error closing\r\n");
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

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent, const MSG_ANTHandler_t * handler){
    self.parent = parent;
    self.user_handler = handler;
    self.message_listener.on_message = _on_message;
    self.message_listener.on_message_sent = _on_message_sent;
    self.tx_queue = hlo_queue_init(64);
    APP_ASSERT(self.tx_queue);
    {
        self.base.init =  _init;
        self.base.flush = _flush;
        self.base.send = _send;
        self.base.destroy = _destroy;
        self.base.type = ANT;
        self.base.typestr = name;
    }
    return &self.base;
}
