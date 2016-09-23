#ifdef ANT_STACK_SUPPORT_REQD  // This is a temp fix because we need to compile on 51822 S110
#include "ant_driver.h"
#include <ant_interface.h>
#include <ant_parameters.h>
#include "util.h"
#include "app.h"

#define HLO_ANT_NETWORK_KEY {0xA8, 0xAC, 0x20, 0x7A, 0x1D, 0x72, 0xE3, 0x4D}
#define HLO_ANT_NETWORK_CHANNEL 66
#define HLO_ANT_NETWORK_PERIOD 128
#define HLO_ANT_NETWORK_PERIOD_BIAS 8
#define HLO_ANT_CHANNEL_EXT_OPT 0
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
    volatile bool reliable_mode;
    const hlo_ant_event_listener_t * event_listener;
}self;

static void _handle_tx(uint8_t channel, const hlo_ant_device_t * dev);
static int _find_open_channel_by_device(const hlo_ant_device_t * device, uint8_t begin, uint8_t end){
    uint8_t i;
    for(i = begin; i <= end; i++){
        uint8_t status = STATUS_UNASSIGNED_CHANNEL;
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
    if(! (ext_fields & EXT_PARAM_ASYNC_TX_MODE) ){
        ret += sd_ant_channel_period_set(channel, phy->period);
    }
    ret += sd_ant_channel_id_set(channel, device->device_number, device->device_type, device->transmit_type);
    ret += sd_ant_channel_low_priority_rx_search_timeout_set(channel, 0xFF);
    ret += sd_ant_channel_rx_search_timeout_set(channel, 0);
    ret += sd_ant_channel_radio_tx_power_set(channel, RADIO_TX_POWER_LVL_2, 0);
    return ret;
}
static int
_configure_channel_as_central(uint8_t channel,const hlo_ant_channel_phy_t * phy,  const hlo_ant_device_t * device, uint8_t ext_fields){
    int ret = 0;
    ret += sd_ant_channel_assign(channel, phy->channel_type, phy->network, ext_fields);
    //ret += sd_ant_channel_id_set(channel, device->device_number, device->device_type, device->transmit_type);
    ret += sd_ant_channel_id_set(channel, device->device_number, device->device_type, device->transmit_type);
    return ret;
}
int32_t hlo_ant_init(hlo_ant_role role, const hlo_ant_event_listener_t * user){
    hlo_ant_channel_phy_t phy = {
        .period = 273,
        .frequency = HLO_ANT_NETWORK_CHANNEL,
        .channel_type = CHANNEL_TYPE_SLAVE,
        .network = 0
    };

    sd_ant_stack_reset();

#ifdef USE_HLO_ANT_NETWORK
    uint8_t network_key[8] = HLO_ANT_NETWORK_KEY;
    sd_ant_network_address_set(0,network_key);
#else
    uint8_t network_key[8] = {0,0,0,0,0,0,0,0};
    sd_ant_network_address_set(0,network_key);
#endif
    hlo_ant_device_t device = {0};
    if(!user){
        return -1;
    }
    self.role = role;
    self.event_listener = user;
    if(role == HLO_ANT_ROLE_CENTRAL){
        APP_OK(sd_ant_lib_config_set(ANT_LIB_CONFIG_MESG_OUT_INC_DEVICE_ID | ANT_LIB_CONFIG_MESG_OUT_INC_RSSI | ANT_LIB_CONFIG_MESG_OUT_INC_TIME_STAMP));
        PRINTS("Configured as ANT Central\r\n");
        APP_OK(_configure_channel(0, &phy, &device, 0)); 
        sd_ant_rx_scan_mode_start(0);
    }
    return 0;
}
int32_t hlo_ant_connect(const hlo_ant_device_t * device, bool reliable){
    //scenarios:
    //no channel with device : create channel, return success
    //channel with device : return success
    //no channel available : return error
    uint8_t begin = (self.role == HLO_ANT_ROLE_CENTRAL)?1:0;
    int ch = _find_open_channel_by_device(device, begin,7);
    if(ch >= begin){
        _handle_tx((uint8_t)ch, device);
        return 0;
    }else{
        //open channel
        int new_ch = _find_unassigned_channel(begin, 7);
        if(new_ch >= begin){
            //bias the period to reduce chance for channel collision
            uint16_t device_period = (HLO_ANT_NETWORK_PERIOD - (HLO_ANT_NETWORK_PERIOD_BIAS/2)) + (device->device_number % HLO_ANT_NETWORK_PERIOD_BIAS);
            hlo_ant_channel_phy_t phy = {
                .period = device_period,
                .frequency = HLO_ANT_NETWORK_CHANNEL,
                .network = 0
            };
            if(reliable){
                phy.channel_type = CHANNEL_TYPE_MASTER;
            }else{
                phy.channel_type = CHANNEL_TYPE_MASTER_TX_ONLY;
            }
            self.reliable_mode = reliable;
            if(self.role == HLO_ANT_ROLE_PERIPHERAL){
                uint8_t opt = HLO_ANT_CHANNEL_EXT_OPT;
                APP_OK(_configure_channel((uint8_t)new_ch, &phy, device, opt));
                if(!(opt & EXT_PARAM_ASYNC_TX_MODE)){
                    APP_OK(sd_ant_channel_open((uint8_t)new_ch));
                }
            }else{
                //as central, we dont connect, but instead start by sending a dud message
                phy.channel_type = CHANNEL_TYPE_SLAVE;
                APP_OK(_configure_channel_as_central((uint8_t)new_ch, &phy, device, 0));
            }
            _handle_tx((uint8_t)new_ch, device);
            return new_ch;
        }
    }
    return -1;
}

int32_t hlo_ant_disconnect(const hlo_ant_device_t * device){
    uint8_t begin = (self.role == HLO_ANT_ROLE_CENTRAL)?1:0;
    int ch = _find_open_channel_by_device(device, begin,7);
    if(ch >= begin){
        /*
         *PRINTS("Closing Channel = ");
         *PRINT_HEX(&ch, 1);
         *PRINTS("\r\n");
         */
        if(self.role == HLO_ANT_ROLE_CENTRAL){
            uint8_t ret;
            sd_ant_pending_transmit_clear (ch, &ret);
            return sd_ant_channel_unassign(ch);
        }else{
            uint8_t opt = HLO_ANT_CHANNEL_EXT_OPT;
            if( opt & EXT_PARAM_ASYNC_TX_MODE ){
                return 0;
            }else{
                return sd_ant_channel_close(ch);
            }
        }
    }
    return -1;
    
}
int32_t hlo_ant_cw_test(uint8_t freq, uint8_t tx_power){
    sd_ant_stack_reset();
    sd_ant_cw_test_mode_init();
    if(tx_power <= RADIO_TX_POWER_LVL_4){
        sd_ant_cw_test_mode(freq, tx_power, 0);
    }else{
        return -1;
    }
    return 0;
}

#define ANT_EXT_DEVICE_TYPE_OFFSET 2
#define ANT_EXT_TRANSMIT_TYPE_OFFSET 3
#define ANT_EXT_MEAS_TYPE_OFFSET 4
#define ANT_EXT_RSSI_OFFSET 5
static bool _parse_device(uint8_t channel, uint8_t * msg_buffer, hlo_ant_device_t * out_device, hlo_ant_role role){
    if(role == HLO_ANT_ROLE_CENTRAL){
        ANT_MESSAGE * msg = (ANT_MESSAGE*)msg_buffer;
        EXT_MESG_BF ext = msg->ANT_MESSAGE_sExtMesgBF;
        uint8_t * extbytes = msg->ANT_MESSAGE_aucExtData;
        if(ext.ucExtMesgBF & MSG_EXT_ID_MASK){
            *out_device = (hlo_ant_device_t){
                .device_number = *((uint16_t*)extbytes),
                    .device_type = extbytes[ANT_EXT_DEVICE_TYPE_OFFSET],
                    .transmit_type = extbytes[ANT_EXT_TRANSMIT_TYPE_OFFSET],
                    .measurement_type = extbytes[ANT_EXT_MEAS_TYPE_OFFSET],
                    .rssi = extbytes[ANT_EXT_RSSI_OFFSET],
            };
            return true;
        }
    }else{
        if(NRF_SUCCESS == sd_ant_channel_id_get(channel, &out_device->device_number, &out_device->device_type, &out_device->transmit_type)){
            return true;
        }

    }
    return false;
}
static void
_handle_rx(uint8_t * msg_buffer, const hlo_ant_device_t * device, bool * ack){
    ANT_MESSAGE * msg = (ANT_MESSAGE*)msg_buffer;
    uint8_t * rx_payload = msg->ANT_MESSAGE_aucPayload;
    self.event_listener->on_rx_event(device, rx_payload, 8, self.role, ack);
}

static void  //peripheral tx mode
_handle_tx(uint8_t channel, const hlo_ant_device_t * dev){
    uint8_t out_buf[8] = {0};
    if(self.event_listener->on_tx_event(dev, out_buf, self.role, self.reliable_mode)){
        if(self.reliable_mode){
            sd_ant_acknowledge_message_tx(channel, 8, out_buf);
        }else{
            sd_ant_broadcast_message_tx(channel, 8, out_buf);
        }
    }
}

void ant_handler(ant_evt_t * p_ant_evt){
    uint8_t event = p_ant_evt->event;
    uint8_t ant_channel = p_ant_evt->channel;
    uint8_t * event_message_buffer = (uint8_t*)p_ant_evt->evt_buffer;
    hlo_ant_device_t dev;
    bool ack = true;
    switch(event){
        case EVENT_RX_FAIL:
            break;
        case EVENT_RX:
            DEBUGS("R");
            if( _parse_device(ant_channel, event_message_buffer, &dev, self.role) ){
                _handle_rx(event_message_buffer, &dev, &ack);
                if(ack && self.role == HLO_ANT_ROLE_CENTRAL){
                    hlo_ant_connect(&dev, true);
                }
            }
            break;
        case EVENT_RX_SEARCH_TIMEOUT:
            DEBUGS("RXTO\r\n");
            break;
        case EVENT_RX_FAIL_GO_TO_SEARCH:
            DEBUGS("RXFTS\r\n");
            break;
        case EVENT_TRANSFER_RX_FAILED:
            DEBUGS("RFFAIL\r\n");
            break;
        case EVENT_TX:
            DEBUGS("T");
            if( _parse_device(ant_channel, event_message_buffer, &dev, self.role) ){
                if(self.role == HLO_ANT_ROLE_PERIPHERAL){
                    _handle_tx(ant_channel, &dev);
                }
            }
            break;
        case EVENT_TRANSFER_TX_FAILED:
            break;
        case EVENT_CHANNEL_COLLISION:
            DEBUGS("XX\r\n");
            break;
        case EVENT_CHANNEL_CLOSED:
            DEBUGS("X");
            sd_ant_channel_unassign(ant_channel);
            break;
        default:
            break;
    }

}

int32_t hlo_ant_pause_radio(void){
    uint8_t status;
    if(self.role == HLO_ANT_ROLE_CENTRAL){
        //TODO only handle radio for central currently
        //Potential issues include pending tx on other channels
        if( NRF_SUCCESS == sd_ant_channel_status_get(0, &status) && status != STATUS_UNASSIGNED_CHANNEL){
            return sd_ant_channel_close(0);
        }else{
            return -1;
        }
    }
    return NRF_SUCCESS;
}
int32_t hlo_ant_resume_radio(void){
    uint8_t status;
    if(self.role == HLO_ANT_ROLE_CENTRAL){
        //TODO only handle radio for central currently
        //Potential issues include pending tx on other channels
        if( NRF_SUCCESS == sd_ant_channel_status_get(0, &status) && status == STATUS_UNASSIGNED_CHANNEL){
            return hlo_ant_init(HLO_ANT_ROLE_CENTRAL,self.event_listener);
        }else{
            return -1;
        }
    }
    return NRF_SUCCESS;
}
#endif
