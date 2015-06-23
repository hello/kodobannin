#include "ble_packet.h"
#include "morpheus_gatt.h"
#include "util.h"
#include <string.h>

#define NUM_SESSIONS 2

typedef struct{
    uint8_t endsq, nesq;
    uint16_t key;
    uint16_t written;
    uint16_t reserved;
    MSG_Data_t * data;
}msg_session_t;

static struct{
    msg_session_t rx_sessions[NUM_SESSIONS];
    ble_packet_message_handler handler;
}self;

void ble_packet_set_handler(ble_packet_message_handler handler){
    self.handler = handler;
}
/*
  uint16_t                    handle;             < Attribute Handle.
  uint8_t                     op;                 < Type of write operation, see @ref BLE_GATTS_OPS.
  ble_gatts_attr_context_t    context;            < Attribute Context
  uint16_t                    offset;             < Offset for the write operation.
  uint16_t                    len;                < Length of the incoming data
  uint8_t                     data[1];            < Incoming data, variable length.
 */
static void _reset_session(msg_session_t * session){
    if(session->data){
        MSG_Base_ReleaseDataAtomic(session->data);
    }
    memset(session, 0, sizeof(*session));
}
static int write_session(msg_session_t * session, uint8_t * data, size_t data_size){
    if(session->data){
        if(session->written + data_size > session->data->len){
            return -1;
        }
        memcpy((uint8_t*)session->data->buf + session->written, data, data_size);
        session->written += data_size;
        return session->written;
    }
    return -2; 
}
static void _handle_message(msg_session_t * session, ble_gatts_evt_write_t * event){
    struct hlo_ble_packet * ble_packet = (struct hlo_ble_packet*)event->data;
    size_t write_len = 0;
    if(ble_packet->sequence_number == 0 && ble_packet->header.packet_count > 0){
        size_t len = 18 + (ble_packet->header.packet_count - 1) * 19;//allocate the worst case size first
        _reset_session(session);
        session->data = MSG_Base_AllocateDataAtomic(len);
        session->endsq = ble_packet->header.packet_count - 1;
        write_len = event->len -1;

        PRINTS("Allocating BLE buffer: ");
        PRINT_DEC(&len, 4);
        PRINTS(" bytes\r\n");

        session->nesq = 1;

    }else if(ble_packet->sequence_number == session->nesq){
        write_len = event->len - 2;
    }else{
        //sequence number mismatch
        _reset_session(session);
        return;
    }

    int total = write_session(session, ble_packet->header.data, write_len);
    
    if(ble_packet->sequence_number == session->endsq){
        if(self.handler){
            session->data->len = (uint16_t)total;//set to actual written data
            self.handler(event->handle, session->data);
        }
        _reset_session(session);
    }

    session->nesq++;
}
void ble_packet_write_handler(ble_gatts_evt_write_t* event){
    int i;
    uint16_t uuid = event->context.char_uuid.uuid;
    PRINTS("BLE Session: ");
    PRINT_HEX(&uuid, 2);
    PRINTS("\r\n");
    for(i = 0; i < NUM_SESSIONS; i++){
        if(self.rx_sessions[i].key == uuid){
            _handle_message(&self.rx_sessions[i], event);
            return;
        }
    }
    for(i = 0; i < NUM_SESSIONS; i++){
        if(!self.rx_sessions[i].key){
            self.rx_sessions[i].key = uuid;
            _handle_message(&self.rx_sessions[i], event);
            return;
        }
    }
    PRINTS("No more sessions available\r\n");
}
