#ifdef ANT_STACK_SUPPORT_REQD  // This is a temp fix because we need to compile on 51822 S110
#include "ant_driver.h"
#include <ant_interface.h>
#include <ant_parameters.h>
#include "util.h"

static struct{
    hlo_ant_role role;
    hlo_ant_event_callback_t * event_listener;
}self;

int32_t hlo_ant_init(hlo_ant_role role, const hlo_ant_event_callback_t * callbacks){
    if(!callbacks){
        return -1;
    }
    self.role = role;
    self.event_listener = callbacks;
    return 0;
}
const hlo_ant_channel_t * hlo_ant_connect(const hlo_ant_device_t * device){

}

int32_t hlo_ant_disconnect(const hlo_ant_channel_t * channel){
    
}

static void
_handle_rx(uint8_t channel, uint8_t * msg_buffer, uint16_t size){
    ANT_MESSAGE * msg = (ANT_MESSAGE*)msg_buffer;
    uint8_t * rx_payload = msg->ANT_MESSAGE_aucPayload;
    hlo_ant_device_t device;
    if(self.role == HLO_ANT_ROLE_CENTRAL){
        EXT_MESG_BF ext = msg->ANT_MESSAGE_sExtMesgBF;
        uint8_t * extbytes = msg->ANT_MESSAGE_aucExtData;
        if(ext.ucExtMesgBF & MSG_EXT_ID_MASK){
            device = (hlo_ant_device_t){
                .device_number = *((uint16_t*)extbytes),
                .device_type = extbytes[4],
                .transmit_type = extbytes[3]
            };
        }else{
            //error
            return;
        }
    }else if(self.role == HLO_ANT_ROLE_PERIPHERAL){
        if(NRF_SUCCESS == sd_ant_channel_id_get(channel, &device.device_number, &device.device_type, &device.transmit_type)){
        }else{
            //error
            return;
        }
    }else{
        return;
    }
    self.event_listener->on_rx_event(&device, rx_payload, size);
}

static void
_handle_tx(uint8_t channel, uint8_t * msg_buffer, uint16_t size){
    hlo_ant_device_t device;
    uint8_t out_buf[8] = {0};
    uint8_t out_size = 8;
    if(channel == 0 && self.role == HLO_ANT_ROLE_CENTRAL){
        //this should not happen
        return;
    }else{
        if(NRF_SUCCESS == sd_ant_channel_id_get(channel, &device.device_number, &device.device_type, &device.transmit_type)){
        }else{
            //error
            return;
        }
    }
    self.event_listener->on_tx_event(&device, out_buf, &out_size);
    if(out_size <= 8){
        sd_ant_broadcast_message_tx(channel, out_size, out_buf);
    }
}

void ant_handler(ant_evt_t * p_ant_evt){
    uint8_t event = p_ant_evt->event;
    uint8_t ant_channel = p_ant_evt->channel;
    uint32_t * event_message_buffer = p_ant_evt->evt_buffer;
    switch(event){
        case EVENT_RX_FAIL:
            break;
        case EVENT_RX:
            PRINTS("R");
            //_handle_rx(&ant_channel,event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
            break;
        case EVENT_RX_SEARCH_TIMEOUT:
            PRINTS("RXTO\r\n");
            break;
        case EVENT_RX_FAIL_GO_TO_SEARCH:
            PRINTS("RXFTS\r\n");
            break;
        case EVENT_TRANSFER_RX_FAILED:
            PRINTS("RFFAIL\r\n");
            break;
        case EVENT_TX:
            PRINTS("T");
            //_handle_tx(&ant_channel, event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
            break;
        case EVENT_TRANSFER_TX_FAILED:
            break;
        case EVENT_CHANNEL_COLLISION:
            PRINTS("XX\r\n");
            break;
        case EVENT_CHANNEL_CLOSED:
            //_handle_channel_closure(&ant_channel, event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
            break;
        default:
            break;
    }

}

#endif
