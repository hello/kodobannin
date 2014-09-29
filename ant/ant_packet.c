#include "ant_packet.h"
#include "util.h"
#include "crc16.h"

#define CID2UID(cid) (cid)
#define UID2CID(uid) ((uint16_t)uid)
#define PACKET_INTEGRITY_CHECK(buf) (buf[1] != 0 && buf[0] <= buf[1])
#define DEFAULT_ANT_RETRANSMIT_COUNT 3

#ifndef ANT_RETRANSMIT_COUNT
#define ANT_RETRANSMIT_COUNT DEFAULT_ANT_RETRANSMIT_COUNT
#endif

typedef struct{
    uint8_t page;
    uint8_t page_count;//non 0 if it's a MSG_Data
    uint8_t payload[6];//unused payload must be set to 0
}hlo_ant_payload_packet_t;

typedef struct{
    uint8_t page;
    uint8_t page_count;
    uint8_t reserved0;
    uint8_t reserved1;
    uint16_t size;
    uint16_t checksum;
}hlo_ant_header_packet_t;

typedef struct{
    uint16_t cid;
    uint16_t rx_count;
    hlo_ant_header_packet_t rx_header;
    hlo_ant_header_packet_t tx_header;
    MSG_Data_t * rx_obj;
    int16_t tx_count;
    MSG_Data_t * tx_obj;
}hlo_ant_packet_session_t;


static struct{
    hlo_ant_event_listener_t cbs;
    hlo_ant_packet_session_t entries[ANT_PACKET_MAX_CONCURRENT_SESSIONS];
    hlo_ant_packet_listener * user;
}self;

static inline uint16_t _calc_checksum(MSG_Data_t * data){
    return (crc16_compute(data->buf, data->len, NULL));
}

static inline DECREF
_reset_session_tx(hlo_ant_packet_session_t * session){
    if(session->tx_obj){
        MSG_Base_ReleaseDataAtomic(session->tx_obj);
    }
    //TODO: trick to sendbunch of headers before the body is to use negative number
    session->tx_obj = NULL;
    session->tx_count = -5;
}
static inline DECREF
_reset_session_rx(hlo_ant_packet_session_t * session){
    if(session->rx_obj){
        MSG_Base_ReleaseDataAtomic(session->rx_obj);
    }
    session->rx_obj = NULL;
    session->rx_count = 0;
}
//finds session, if not exist allocates one
static inline hlo_ant_packet_session_t *
_acquire_session(const hlo_ant_device_t * device){
    int i;
    uint16_t cid = device->device_number;
    for(i = 0; i < ANT_PACKET_MAX_CONCURRENT_SESSIONS; i++){
        if(self.entries[i].cid == cid){
            return &(self.entries[i]);
        }
    }
    for(i = 0; i < ANT_PACKET_MAX_CONCURRENT_SESSIONS; i++){
        if(self.entries[i].cid == 0){
            self.entries[i].cid = cid;
            _reset_session_tx(&(self.entries[i]));
            _reset_session_rx(&(self.entries[i]));
            return &(self.entries[i]);
        }
    }
    return NULL;
}
static inline
_close_session(hlo_ant_packet_session_t * session){
    _reset_session_rx(session);
    _reset_session_tx(session);
    session->cid = 0;
}

static uint8_t _assemble_rx_payload(MSG_Data_t * payload, const hlo_ant_payload_packet_t * packet){
   uint16_t offset = (packet->page - 1) * 6; 
   //make sure the offset does not exceed the length
   if(offset + 6 <= payload->len){
       uint8_t i;
       for(i = 0; i < 6; i++){
           payload->buf[offset + i] = packet->payload[i];
       }
   }
   return 0;
}
static MSG_Data_t * _assemble_rx(hlo_ant_packet_session_t * session, uint8_t * buffer, uint8_t len){
    if(PACKET_INTEGRITY_CHECK(buffer)){
        if(buffer[0] == 0){
            //header
            uint16_t new_crc = (uint16_t)(buffer[7] << 8) + buffer[6];
            uint16_t new_size = (uint16_t)(buffer[5] << 8) + buffer[4];
            hlo_ant_header_packet_t * new_header = buffer;
            if(new_crc != session->rx_header.checksum){
                //if crc is new, create new obj
                //TODO optimize by not swapping objects, but reusing it
                session->rx_header = *new_header;
                _reset_session_rx(session);
                session->rx_obj = MSG_Base_AllocateDataAtomic(new_size);
            }
        }else if(session->rx_obj){
            _assemble_rx_payload(session->rx_obj, (hlo_ant_payload_packet_t *)buffer);
            if(session->rx_count >= (session->rx_header.page_count)){
                if(_calc_checksum(session->rx_obj) == session->rx_header.checksum){
                    return session->rx_obj;
                }
            }
        }
        session->rx_count++;
    }else{
        //invalid packet
    }
    return NULL;
}
static void _handle_tx(const hlo_ant_device_t * device, uint8_t * out_buffer, uint8_t * out_buffer_len){
    hlo_ant_packet_session_t * session = _acquire_session(device);
    if(session->tx_obj){
        *out_buffer_len = 8;
        if(session->tx_count < 0){
            //copy header to prime transmission
            memcpy(out_buffer, &session->tx_header, 8);
        }else if(session->tx_count < (session->tx_header.page_count * ANT_RETRANSMIT_COUNT)){
            uint16_t mod = session->tx_count % session->tx_header.page_count;
            if(mod == 0){
                memcpy(out_buffer, &session->tx_header, 8);
            }else{
                uint16_t offset = (mod - 1) * 6;
                uint16_t i;
                out_buffer[0] = mod;
                out_buffer[1] = session->tx_header.page_count;
                for(i = 0; i < 6; i++){
                    if(offset + i < session->tx_obj->len){
                        out_buffer[2+i] = session->tx_obj->buf[offset+i];
                    }
                }
            }
        }else{
            MSG_Data_t * sent_obj = session->tx_obj;
            MSG_Base_AcquireDataAtomic(sent_obj);
            //reset to prepare ahead of time if user has any more data to be sent
            _reset_session_tx(session);
            if(self.user && self.user->on_message_sent)
                self.user->on_message_sent(device, sent_obj);
            MSG_Base_ReleaseDataAtomic(sent_obj);
            return;
        }
        session->tx_count++;
    }
}


static void _handle_rx(const hlo_ant_device_t * device, uint8_t * buffer, uint8_t buffer_len){
    hlo_ant_packet_session_t * session = _acquire_session(device);
    if(session){
        //TODO handle timeout on session
        MSG_Data_t * ret_obj = _assemble_rx(session, buffer, buffer_len);
        if(ret_obj){
            if(self.user && self.user->on_message){
                self.user->on_message(device, ret_obj);
            }
            _reset_session_rx(session);
        }
    }else{
        //no more sessions available
    }

}
static void _handle_error(const hlo_ant_device_t * device, uint32_t event){

}

hlo_ant_event_listener_t * hlo_ant_packet_init(const hlo_ant_packet_listener * user_listener){
    self.cbs.on_tx_event = _handle_tx;
    self.cbs.on_rx_event = _handle_rx;
    self.cbs.on_error_event = _handle_error;
    self.user = user_listener;
    return &self.cbs;
}
int hlo_ant_packet_send_message(const hlo_ant_device_t * device, MSG_Data_t * msg){
    if(msg){
        hlo_ant_packet_session_t * session = _acquire_session(device);
        if(session){
            _reset_session_tx(session);
            session->tx_obj = msg;
            MSG_Base_AcquireDataAtomic(msg);
            session->tx_header.size = msg->len;
            session->tx_header.checksum = _calc_checksum(msg);
            session->tx_header.page = 0;
            session->tx_header.page_count = msg->len / 6 + ((msg->len % 6) ? 1 : 0);
            return hlo_ant_connect(device);
        }else{
            PRINTS("Session Full \r\n");
        }
    }
    return -1;
}
