#include "ant_packet.h"
#include "util.h"
#include "crc16.h"
#include <string.h>

#define CID2UID(cid) (cid)
#define UID2CID(uid) ((uint16_t)uid)
#define DEFAULT_ANT_RETRANSMIT_COUNT 4

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
    struct{
        uint8_t page;
        uint8_t retry;
    }lockstep;
    hlo_ant_header_packet_t rx_header;
    hlo_ant_header_packet_t tx_header;
    MSG_Data_t * rx_obj;
    MSG_Data_t * tx_obj;
    uint32_t age;
}hlo_ant_packet_session_t;

static struct{
    hlo_ant_event_listener_t cbs;
    hlo_ant_packet_session_t entries[ANT_PACKET_MAX_CONCURRENT_SESSIONS];
    const hlo_ant_packet_listener * user;
    uint32_t global_age;
}self;

static inline uint16_t _calc_checksum(const MSG_Data_t * data){
    return (crc16_compute(data->buf, data->len, NULL));
}

static inline void
_reset_lockstep(hlo_ant_packet_session_t * session){
    session->lockstep.retry = DEFAULT_ANT_RETRANSMIT_COUNT;
    session->lockstep.page = 0;
}

static inline DECREF void
_reset_tx_obj(hlo_ant_packet_session_t * session){
    if(session->tx_obj){
        MSG_Base_ReleaseDataAtomic(session->tx_obj);
    }
    session->tx_obj = NULL;
}
static inline DECREF void
_reset_rx_obj(hlo_ant_packet_session_t * session){
    if(session->rx_obj){
        MSG_Base_ReleaseDataAtomic(session->rx_obj);
    }
    session->rx_obj = NULL;
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
            _reset_tx_obj(&(self.entries[i]));
            _reset_rx_obj(&(self.entries[i]));
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
                _reset_rx_obj(&(self.entries[i]));
                self.entries[i].age = self.global_age;
                self.entries[i].cid = cid;
                return &(self.entries[i]);
            }
        }
    }
    return NULL;
}

static uint8_t _assemble_rx_payload(MSG_Data_t * payload, const hlo_ant_payload_packet_t * packet){
   uint16_t offset = (packet->page - 1) * 6; 
   //make sure the offset does not exceed the length
   uint8_t i;
   for(i = 0; i < payload->len - offset; i++){
       payload->buf[offset + i] = packet->payload[i];
   }
   return 0;
}
static MSG_Data_t * _assemble_rx(hlo_ant_packet_session_t * session, uint8_t * buffer, uint8_t len, bool * out_has_new_obj){
    //this function still uses the legacy way of counting packets (total received >= header max -> crc check -> produce object)
    //rather than using lockstep mode (rx page == header max) for backward compatibility reasons
    hlo_ant_payload_packet_t * packet = (hlo_ant_payload_packet_t*)buffer;

    if(packet->page == 0 && packet->page_count > 0){
    //1. check if it's a header packet
        //1.a now check if it's the same object as before
        uint16_t new_crc = (uint16_t)(buffer[7] << 8) | buffer[6];
        if( new_crc != session->rx_header.checksum ){
            memcpy(&session->rx_header, buffer, sizeof(hlo_ant_header_packet_t));
            if(session->rx_header.size <= MSG_Base_FreeCount() && session->rx_header.size != 0){
                _reset_rx_obj(session);//this is just to refresh any stale objects that hasn't been completed
                session->rx_obj = MSG_Base_AllocateDataAtomic(session->rx_header.size);
                if ( out_has_new_obj ){
                    *out_has_new_obj = true;
                }
            }
        }

    }else if(session->rx_obj && packet->page && packet->page <= session->rx_header.page_count){
    //2. if an object already exists, and the bounds make sense
        _assemble_rx_payload(session->rx_obj,packet);
        if(_calc_checksum(session->rx_obj) == session->rx_header.checksum){
            return session->rx_obj;
        }
    }
    return NULL;
}
static void _write_buffer(const hlo_ant_packet_session_t * session, uint8_t * out_buffer){
    int i;
    if(session->lockstep.page == 0){
        memcpy(out_buffer, &session->tx_header, 8);
    }else{
        out_buffer[0] = session->lockstep.page;
        out_buffer[1] = session->tx_header.page_count;
        uint16_t offset = (session->lockstep.page - 1) * 6;
        for(i = 0; i < 6; i++){
            if(offset + i < session->tx_obj->len){
                out_buffer[2+i] = session->tx_obj->buf[offset+i];
            }else{
                out_buffer[2+i] = 0;
            }
        }
    }

}
static bool _handle_tx(const hlo_ant_device_t * device, uint8_t * out_buffer, hlo_ant_role role){
    hlo_ant_packet_session_t * session = _acquire_session(device);
    if(!session){
        return false;
    }
    /*
     *PRINTS("t:");
     *PRINT_HEX(&session->lockstep.page, 1);
     *PRINTS("\r\n");
     */
    out_buffer[0] = session->lockstep.page;

    if(role == HLO_ANT_ROLE_CENTRAL){
        if(session->tx_obj){
            _write_buffer(session, out_buffer);
        }
    }else if(role == HLO_ANT_ROLE_PERIPHERAL){
        if(session->lockstep.retry--){
            if(session->tx_obj){
                if( session->lockstep.page <= session->tx_header.page_count ){
                    _write_buffer(session, out_buffer);
                }else{
                    self.user->on_message_sent(device, session->tx_obj);
                    _reset_tx_obj(session);
                }
            }else if(!session->rx_obj){
                return false;
            }
        }else{
            if(session->tx_obj){
                self.user->on_message_failed(device, session->tx_obj);
                _reset_tx_obj(session);
            }
            return false;
        }
    }
    return true;
}

static void _set_header(hlo_ant_header_packet_t * header, const MSG_Data_t * msg){
    memset(header, 0, sizeof(*header));
    header->size = msg->len;
    header->checksum = _calc_checksum(msg);
    header->page = 0;
    header->page_count = msg->len / 6 + ((msg->len % 6) ? 1 : 0);
}

static void _handle_rx(const hlo_ant_device_t * device, uint8_t * buffer, uint8_t buffer_len, hlo_ant_role role){
    hlo_ant_packet_session_t * session = _acquire_session(device);
    hlo_ant_payload_packet_t * packet = (hlo_ant_payload_packet_t*)buffer;
    bool new_obj = false;//flag that indicates a new object is allocated(connected)
    if(!session){
        return;
    }
    /*
     *PRINTS("r:");
     *PRINT_HEX(&packet->page, 1);
     *PRINTS("/");
     *PRINT_HEX(&packet->page_count, 1);
     *PRINTS("\r\n");
     */
    //legacy handling mode, we don't care about lockstep on rx here
    //since delivery is checked by crc
    MSG_Data_t * ret_obj = _assemble_rx(session, buffer, buffer_len, &new_obj);
    if ( ret_obj ){
        self.user->on_message(device, ret_obj);
        _reset_rx_obj(session);
    }

    if(role == HLO_ANT_ROLE_CENTRAL){//central receives first, then transmits
        if( !session->tx_obj && new_obj){
            MSG_Data_t * ret = self.user->on_connect(device);
            if(ret){
                _set_header(&session->tx_header, ret);
                session->tx_obj = ret;
            }
        }
        if ( packet->page == session->lockstep.page || packet->page == (session->lockstep.page + 1) ){
            //good path for page counter
            if ( session->tx_obj && packet->page > session->tx_header.page_count ){
                self.user->on_message_sent(device, session->tx_obj);
                _reset_tx_obj(session);
            }
        }else if (session->tx_obj){
            //desync has happened
            self.user->on_message_failed(device, session->tx_obj);
            _reset_tx_obj(session);
        }
        //as central, we always ack back what we receive
        session->lockstep.page = packet->page;
    }else if(role == HLO_ANT_ROLE_PERIPHERAL) {
        if( packet->page == session->lockstep.page ){
            session->lockstep.page++;
            session->lockstep.retry = DEFAULT_ANT_RETRANSMIT_COUNT;
        }
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
            _reset_tx_obj(session);
            _reset_lockstep(session);
            session->tx_obj = msg;
            MSG_Base_AcquireDataAtomic(msg);
            _set_header(&session->tx_header, msg);
            return hlo_ant_connect(device);
        }else{
            PRINTS("Session Full \r\n");
            return -2;
        }
    }
    return -1;
}
