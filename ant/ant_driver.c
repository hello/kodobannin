#ifdef ANT_STACK_SUPPORT_REQD  // This is a temp fix because we need to compile on 51822 S110
#include "ant_driver.h"
#include <ant_interface.h>
#include <ant_parameters.h>
#include "util.h"

#define ANT_EVENT_MSG_BUFFER_MIN_SIZE 32
typedef struct{
    //cached status
    uint8_t reserved;
    //MASTER/SLAVE/SHARED etc
    uint8_t channel_type;
    //2.4GHz + frequency, 2.4XX
    uint8_t frequency;
    //network key, HLO specific
    uint8_t network;
    //period 32768/period Hz
    uint16_t period;
}hlo_ant_channel_phy_t;

static struct{
    hlo_ant_role role;
    hlo_ant_event_listener_t * event_listener;
}self;

static int _find_open_channel_by_device(const hlo_ant_device_t * device, uint8_t begin, uint8_t end){
    uint8_t i;
    for(i = begin; i <= end; i++){
        uint8_t status;
        if(NRF_SUCCESS == sd_ant_channel_status_get(i, &status) && status > STATUS_UNASSIGNED_CHANNEL){
            uint16_t dev_num;
            uint8_t dud;
            if(NRF_SUCCESS == sd_ant_channel_id_get(i, &dev_num, &dud, &dud)){
                if(device->device_number == dev_num){
                    return i;
                }
            }
        }
    }
    return -1;
}
static int _find_unassigned_channel(uint8_t begin, uint8_t end){
    uint8_t i;
    for(i = begin; i <= end; i++){
        uint8_t status;
        if( NRF_SUCCESS == sd_ant_channel_status_get(i, &status) && status == STATUS_UNASSIGNED_CHANNEL){
            return i;
        }
    }
    return -1;
}
static int
_configure_channel(uint8_t channel,const hlo_ant_channel_phy_t * phy,  const hlo_ant_device_t * device, uint8_t ext_fields){
    int ret = 0;
    ret += sd_ant_channel_assign(channel, phy->channel_type, phy->network, ext_fields);
    ret += sd_ant_channel_radio_freq_set(channel, phy->frequency);
    ret += sd_ant_channel_period_set(channel, phy->period);
    ret += sd_ant_channel_id_set(channel, device->device_number, device->device_type, device->transmit_type);
    ret += sd_ant_channel_low_priority_rx_search_timeout_set(channel, 0xFF);
    ret += sd_ant_channel_rx_search_timeout_set(channel, 0);
    return ret;
}
int32_t hlo_ant_init(hlo_ant_role role, const hlo_ant_event_listener_t * user){
    hlo_ant_channel_phy_t phy = {
        .period = 273,
        .frequency = 66,
        .channel_type = CHANNEL_TYPE_SLAVE,
        .network = 0
    };
    uint8_t network_key[8] = {0,0,0,0,0,0,0,0};
    sd_ant_stack_reset();
    sd_ant_network_address_set(0,network_key);
    hlo_ant_device_t device = {0};
    if(!user){
        return -1;
    }
    self.role = role;
    self.event_listener = user;
    if(role == HLO_ANT_ROLE_CENTRAL){
        PRINTS("Configured as ANT Central\r\n");
        APP_OK(_configure_channel(0, &phy, &device, 0)); 
        APP_OK(sd_ant_lib_config_set(ANT_LIB_CONFIG_MESG_OUT_INC_DEVICE_ID | ANT_LIB_CONFIG_MESG_OUT_INC_RSSI | ANT_LIB_CONFIG_MESG_OUT_INC_TIME_STAMP));
        sd_ant_rx_scan_mode_start(0);
    }
    return 0;
}
int32_t hlo_ant_connect(const hlo_ant_device_t * device){
    //scenarios:
    //no channel with device : create channel, return success
    //channel with device : return success
    //no channel available : return error
    uint8_t begin = (self.role == HLO_ANT_ROLE_CENTRAL)?1:0;
    int ch = _find_open_channel_by_device(device, begin,7);
    if(ch >= begin){
        PRINTS("Channel already open!\r\n");
        return 0;
    }else{
        //open channel
        int new_ch = _find_unassigned_channel(begin, 7);
        if(new_ch >= begin){
            hlo_ant_channel_phy_t phy = {
                //TODO set period properly based on deivce number
                .period = 1092,
                .frequency = 66,
                .channel_type = (self.role==HLO_ANT_ROLE_CENTRAL)?CHANNEL_TYPE_SLAVE:CHANNEL_TYPE_MASTER,
                .network = 0
            };
            APP_OK(_configure_channel((uint8_t)new_ch, &phy, device, 0));
            if(self.role == HLO_ANT_ROLE_PERIPHERAL){
                APP_OK(sd_ant_channel_open((uint8_t)new_ch));
            }else{
                //as master, we dont connect, but instead start by sending a dud message
                uint8_t message[8] = {0};
                sd_ant_broadcast_message_tx((uint8_t)new_ch, sizeof(message), message);
            }
            return new_ch;
        }
    }
    return -1;
}

int32_t hlo_ant_disconnect(const hlo_ant_device_t * device){
    uint8_t begin = (self.role == HLO_ANT_ROLE_CENTRAL)?1:0;
    int ch = _find_open_channel_by_device(device, begin,7);
    if(ch >= begin){
        sd_ant_channel_close(ch);
        sd_ant_channel_unassign(ch);
        return 0;
    }
    return -1;
    
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
    self.event_listener->on_rx_event(&device, rx_payload, 8);
}

static void
_handle_tx(uint8_t channel, uint8_t * msg_buffer, uint16_t size){
    hlo_ant_device_t device;
    uint8_t out_buf[8] = {0};
    uint8_t out_size = 0;
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
    if(out_size && out_size <= 8){
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
            _handle_rx(ant_channel,event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
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
            _handle_tx(ant_channel, event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
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
