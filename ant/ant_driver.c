#ifdef ANT_STACK_SUPPORT_REQD  // This is a temp fix because we need to compile on 51822 S110
#include "ant_driver.h"
#include <ant_interface.h>
#include <ant_parameters.h>
#include "util.h"

static struct{
    hlo_ant_channel_t channels[8];
    hlo_ant_role role;
    hlo_ant_event_callback_t * event_listener;
}self;

int32_t hlo_ant_init(hlo_ant_role role, const hlo_ant_event_callback_t * callbacks){
    self.role = role;
    self.event_listener = callbacks;
    return 0;
}
const hlo_ant_channel_t * hlo_ant_connect(const hlo_ant_device_t * device){
}
int32_t hlo_ant_disconnect(const hlo_ant_channel_t * channel){
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
