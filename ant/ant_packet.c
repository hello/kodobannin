#include "ant_packet.h"
#include "util.h"
#include "crc16.h"
#include <string.h>

#define CID2UID(cid) (cid)
#define UID2CID(uid) ((uint16_t)uid)
#define DEFAULT_ANT_RETRANSMIT_COUNT 3

//defines how old a session can be before getting swept
#define ANT_SESSION_AGE_LIMIT 4

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
    MSG_Data_t * tx_obj;
    uint32_t age;
    struct{
        uint8_t page;
        uint8_t retry;
        uint8_t status;
    }lockstep;
}hlo_ant_packet_session_t;

#define LOCKSTEP_STATUS_DESYNC (1 << 0)


static struct{
    hlo_ant_event_listener_t cbs;
    hlo_ant_packet_session_t entries[ANT_PACKET_MAX_CONCURRENT_SESSIONS];
    const hlo_ant_packet_listener * user;
    uint32_t global_age;
}self;

static inline uint16_t _calc_checksum(MSG_Data_t * data){
    return (crc16_compute(data->buf, data->len, NULL));
}

static inline DECREF void
_reset_session_tx(hlo_ant_packet_session_t * session){
    if(session->tx_obj){
        MSG_Base_ReleaseDataAtomic(session->tx_obj);
    }
    //TODO: trick to sendbunch of headers before the body is to use negative number
    session->tx_obj = NULL;
}
static inline DECREF void
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
    ++self.global_age;
    //look for existing session
    for(i = 0; i < ANT_PACKET_MAX_CONCURRENT_SESSIONS; i++){
        if(self.entries[i].cid == cid){
            self.entries[i].age = self.global_age;
            return &(self.entries[i]);
        }
    }
    //look for new session
    for(i = 0; i < ANT_PACKET_MAX_CONCURRENT_SESSIONS; i++){
        if(self.entries[i].cid == 0){
            self.entries[i].cid = cid;
            self.entries[i].age = self.global_age;
            _reset_session_tx(&(self.entries[i]));
            _reset_session_rx(&(self.entries[i]));
            return &(self.entries[i]);
        }
    }
    //look for sessions with no tx or rx objects(expired session)
    for(i = 0; i < ANT_PACKET_MAX_CONCURRENT_SESSIONS; i++){
        if(self.entries[i].cid != 0 && !self.entries[i].rx_obj && !self.entries[i].tx_obj){
            self.entries[i].age = self.global_age;
            self.entries[i].cid = cid;
            return &(self.entries[i]);
        }
    }
    //lastly, try look for session with rx object that has age exceeds AGE limit
    for(i = 0; i < ANT_PACKET_MAX_CONCURRENT_SESSIONS; i++){
        if(self.entries[i].cid != 0 && !self.entries[i].tx_obj){
            if(self.global_age - self.entries[i].age >= ANT_SESSION_AGE_LIMIT){
                _reset_session_rx(&(self.entries[i]));
                self.entries[i].age = self.global_age;
                self.entries[i].cid = cid;
                return &(self.entries[i]);
            }
        }
    }
    return NULL;
}
static inline void
_close_session(hlo_ant_packet_session_t * session){
    _reset_session_rx(session);
    _reset_session_tx(session);
    session->cid = 0;
}

static uint8_t _assemble_rx_payload(MSG_Data_t * payload, const hlo_ant_payload_packet_t * packet){
   uint16_t offset = (packet->page - 1) * 6; 
   //make sure the offset does not exceed the length
   uint8_t i;
   for(i = 0; i < (payload->len - offset); i++){
       payload->buf[offset + i] = packet->payload[i];
   }
   return 0;
}
static MSG_Data_t * _assemble_rx(hlo_ant_packet_session_t * session, uint8_t * buffer, uint8_t len){
    //this function still uses the legacy way of counting packets (total received >= header max -> crc check -> produce object)
    //rather than using lockstep mode (rx page == header max) for backward compatibility reasons
    hlo_ant_payload_packet_t * packet = (hlo_ant_payload_packet_t*)buffer;
    //updates the meta counter
    session->rx_count++;

    if( packet->page <= packet->page_count ){//packet can be assembled
        if(packet->page == 0){
            //header
            uint16_t new_crc = (uint16_t)(buffer[7] << 8) | buffer[6];
            uint16_t new_size = (uint16_t)(buffer[5] << 8) | buffer[4];
            
            if( new_size > MSG_Base_FreeCount() || new_size == 0) {
                _reset_session_rx(session);
                return NULL;
            }
            if(new_crc != session->rx_header.checksum){
                //if crc is new, create new obj
                //TODO optimize by not swapping objects, but reusing it
                memcpy(&session->rx_header, buffer, sizeof(hlo_ant_header_packet_t));
                _reset_session_rx(session);
                session->rx_obj = MSG_Base_AllocateDataAtomic(new_size);
            }
        }else if(session->rx_obj){
            _assemble_rx_payload(session->rx_obj,packet);
            if(session->rx_count >= (session->rx_header.page_count)){
                if(_calc_checksum(session->rx_obj) == session->rx_header.checksum){
                    return session->rx_obj;
                }
            }
        }
    }
    return NULL;
}
static void _handle_tx(const hlo_ant_device_t * device, uint8_t * out_buffer, uint8_t * out_buffer_len, hlo_ant_role role){
    hlo_ant_packet_session_t * session = _acquire_session(device);
    if(session){//always ack if nothing to send
        PRINTS("t:");
        PRINT_HEX(&session->lockstep.page, 1);
        PRINTS("\r\n");
        *out_buffer_len = 8;
        out_buffer[0] = session->lockstep.page;
    }else{
        PRINTS("no session\r\n");
    }
    if(session->tx_obj){
        //always send out the maximum length
        *out_buffer_len = 8;
        //we always transmit a minimum number of packets regardless of packet size to ensure delivery
        /*
         *if(session->tx_count < transmit_mark){
         *    uint16_t mod = session->tx_count % (session->tx_header.page_count+1);
         *    if(mod == 0){
         *        memcpy(out_buffer, &session->tx_header, 8);
         *    }else{
         *        uint16_t offset = (mod - 1) * 6;
         *        uint16_t i;
         *        out_buffer[0] = mod;
         *        out_buffer[1] = session->tx_header.page_count;
         *        for(i = 0; i < 6; i++){
         *            if(offset + i < session->tx_obj->len){
         *                out_buffer[2+i] = session->tx_obj->buf[offset+i];
         *            }else{
         *                out_buffer[2+i] = 0;
         *            }
         *        }
         *    }
         *}else{
         *    MSG_Data_t * sent_obj = session->tx_obj;
         *    MSG_Base_AcquireDataAtomic(sent_obj);
         *    //reset to prepare ahead of time if user has any more data to be sent
         *    _reset_session_tx(session);
         *    if(self.user && self.user->on_message_sent)
         *        self.user->on_message_sent(device, sent_obj);
         *    MSG_Base_ReleaseDataAtomic(sent_obj);
         *    return;
         *}
         *session->tx_count++;
         */
    }
}

static void _handle_rx(const hlo_ant_device_t * device, uint8_t * buffer, uint8_t buffer_len, hlo_ant_role role){
    hlo_ant_packet_session_t * session = _acquire_session(device);
    hlo_ant_payload_packet_t * packet = (hlo_ant_payload_packet_t*)buffer;
    if(session){
        MSG_Data_t * ret_obj = _assemble_rx(session, buffer, buffer_len);
        if(ret_obj){
            if(self.user && self.user->on_message){
                self.user->on_message(device, ret_obj);
            }
            _reset_session_rx(session);
        }
        if( role == HLO_ANT_ROLE_CENTRAL ){
            PRINTS("r:");
            PRINT_HEX(&packet->page, 1);
            PRINTS("\r\n");
            if ( !packet->page ){
                session->lockstep.page = packet->page;  //rx means peripheral has initiated a transmission
            }else if( packet->page == (session->lockstep.page + 1)){
                session->lockstep.page++;  //if peripheral has increased its page count, we also increase it
            }else if( packet->page == session->lockstep.page) {
                //retransmit is needed
            }else{
                session->lockstep.status |= LOCKSTEP_STATUS_DESYNC;
                PRINTS("desync\r\n");
            }
            PRINTS("t:");
            PRINT_HEX(&session->lockstep.page, 1);
            PRINTS("\r\n");
            //set up transmit
        }else if(role == HLO_ANT_ROLE_PERIPHERAL){
            if ( packet->page == session->lockstep.page || !session->lockstep.retry){
                session->lockstep.page = session->lockstep.page + 1; //rx means central has acked our last packet
                session->lockstep.retry = DEFAULT_ANT_RETRANSMIT_COUNT;
            }else if(session->lockstep.retry--){
                //will always attempt to retransmit, up to N times
                PRINTS("miss\r\n");
            }
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
        if(session && !session->tx_obj){
            _reset_session_tx(session);
            session->tx_obj = msg;
            MSG_Base_AcquireDataAtomic(msg);
            memset(&session->tx_header, 0, sizeof(session->tx_header));
            session->tx_header.size = msg->len;
            session->tx_header.checksum = _calc_checksum(msg);
            session->tx_header.page = 0;
            session->tx_header.page_count = msg->len / 6 + ((msg->len % 6) ? 1 : 0);
            return hlo_ant_connect(device);
        }else{
            PRINTS("Session Full \r\n");
            return -2;
        }
    }
    return -1;
}
