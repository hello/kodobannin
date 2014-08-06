#include <ant_interface.h>
#include <app_timer.h>
#include <app_error.h>
#include <ant_parameters.h>
#include "message_ant.h"
#include "util.h"
#include "app.h"

#define NUM_ANT_CHANNELS 8
#define ANT_EVENT_MSG_BUFFER_MIN_SIZE 32u  /**< Minimum size of an ANT event message buffer. */

typedef struct{
    union{
        struct{

        }peripheral_info;
        struct{

        }central_info;
        uint8_t buf[8];
    }
}ANT_AirPacket_t;


static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    uint8_t discovery_role;
    app_timer_id_t discovery_timeout;
    ANT_DiscoveryProfile_t profile;
}self;
static char * name = "ANT";
#define CHANNEL_NUM_CHECK(ch) (ch < NUM_ANT_CHANNELS)

static MSG_Status
_destroy_channel(uint8_t channel){
    //this destroys the channel regardless of what state its in
    sd_ant_channel_close(channel);
    sd_ant_channel_unassign(channel);
    return SUCCESS;
}
/*
 *#define STATUS_UNASSIGNED_CHANNEL                  ((uint8_t)0x00) ///< Indicates channel has not been assigned.
 *#define STATUS_ASSIGNED_CHANNEL                    ((uint8_t)0x01) ///< Indicates channel has been assigned.
 *#define STATUS_SEARCHING_CHANNEL                   ((uint8_t)0x02) ///< Indicates channel is active and in searching state.
 *#define STATUS_TRACKING_CHANNEL                    ((uint8_t)0x03) ///< Indicates channel is active and in tracking state.
 */
static uint8_t
_get_channel_status(uint8_t channel){
    uint8_t ret;
    sd_ant_channel_status_get(channel, &ret);
    return ret;
}
static MSG_Status
_connect(uint8_t channel, const ANT_ChannelID_t * id){
    //needs at least assigned
    MSG_Status ret = FAIL;
    if(_get_channel_status(channel)){
        sd_ant_channel_close(channel);
        sd_ant_channel_id_set(channel, id->device_number, id->device_type, id->transmit_type);
        if(!sd_ant_channel_open(channel)){
            ret = SUCCESS;
        }
    }
    return ret;
}

static MSG_Status
_disconnect(uint8_t channel){
    if(!sd_ant_channel_close(channel)){
        return SUCCESS;
    }
    return FAIL;
}

static MSG_Status
_configure_channel(uint8_t channel, const ANT_ChannelPHY_t * spec){
    MSG_Status ret = SUCCESS;
    if(_get_channel_status(channel)){
        ret = _destroy_channel(channel);
    }
    if(!ret){
        ret += sd_ant_channel_assign(channel, spec->channel_type, spec->network, 0);
        ret += sd_ant_channel_radio_freq_set(channel, spec->frequency);
        ret += sd_ant_channel_period_set(channel, spec->period);
    }
    return ret;
}

static MSG_Status
_set_discovery_mode(uint8_t role){
    ANT_ChannelID_t id = {0};
    ANT_ChannelPHY_t phy = { 
        .period = 32768,
        .frequency = 66,
        .channel_type = CHANNEL_TYPE_SLAVE,
        .network = 0};
    if(role != self.discovery_role){
        app_timer_stop(self.discovery_timeout);
        _destroy_channel(ANT_DISCOVERY_CHANNEL);
        self.discovery_role = role;
    }else if(_get_channel_status(ANT_DISCOVERY_CHANNEL)>1){
        return SUCCESS;
    }
    switch(role){
        case 0:
            //central mode
            PRINTS("SLAVE\r\n");
            phy.period = 1092;
            _configure_channel(ANT_DISCOVERY_CHANNEL, &phy);
            _connect(ANT_DISCOVERY_CHANNEL, &id);
            break;
        case 1:
            //peripheral mode
            //configure shit here
            PRINTS("MASTER\r\n");
            phy.channel_type = CHANNEL_TYPE_MASTER;
            phy.period = 1092;
            //set up id
            {
                //test only
                //id.transmit_type = ANT_TRANS_TYPE_2_BYTE_SHARED_ADDRESS;
                id.transmit_type = 3;
                id.device_type = 1;
                id.device_number = 0x5354;
            }
            _configure_channel(ANT_DISCOVERY_CHANNEL, &phy);
            _connect(ANT_DISCOVERY_CHANNEL, &id);
            {
                uint32_t to = APP_TIMER_TICKS(2000, APP_TIMER_PRESCALER);
                app_timer_start(self.discovery_timeout,to, NULL);
            }
            break;
        default:
            break;
    }
    return SUCCESS;
}
/**
 * Handles the timeout of discovery_mode
 */
static void
_discovery_timeout(void * ctx){
    PRINTS("Good Night Sweet Prince\r\n");
    self.discovery_role = 0xFF;
    _destroy_channel(ANT_DISCOVERY_CHANNEL);
}

static void
_handle_channel_closure(uint8_t * channel, uint8_t * buf, uint8_t buf_size){
    PRINTS("Channel Close\r\n");
    if(*channel == ANT_DISCOVERY_CHANNEL){
        PRINTS("Re-opening Channel ");
        PRINT_HEX(channel, 1);
        PRINTS("\r\n");
        MSG_SEND(self.parent,ANT,ANT_SET_ROLE,&self.discovery_role,1);
    }
}

static void
_handle_tx(uint8_t * channel, uint8_t * buf, uint8_t buf_size){
    static uint8_t message[ANT_STANDARD_DATA_PAYLOAD_SIZE] = {0,1,2,3,4,5,6,7};
    uint32_t ret;
    if(*channel == ANT_DISCOVERY_CHANNEL){
        *((uint16_t*)message) = (uint16_t)0x5354;
        message[7]++;
//        PRINT_HEX(message, ANT_STANDARD_DATA_PAYLOAD_SIZE);
        ret = sd_ant_broadcast_message_tx(0,sizeof(message), message);
        PRINTS("Ret = ");
        PRINT_HEX(&ret, 2);
    }
}
static void
_handle_rx(uint8_t * channel, uint8_t * buf, uint8_t buf_size){
    ANT_MESSAGE * msg = (ANT_MESSAGE*)buf;
    PRINT_HEX(msg->ANT_MESSAGE_aucPayload,ANT_STANDARD_DATA_PAYLOAD_SIZE);
    if(*channel == ANT_DISCOVERY_CHANNEL){
        //discovery mode channel
        uint16_t dev_id;
        uint8_t dev_type;
        uint8_t transmit_type;
        PRINTS(" CH: ");
        if(!sd_ant_channel_id_get(*channel, &dev_id, &dev_type, &transmit_type)){
            PRINT_HEX(&dev_id, sizeof(dev_id));
            PRINTS(" | ");
            PRINT_HEX(&dev_type, sizeof(dev_type));
            PRINTS(" | ");
            PRINT_HEX(&transmit_type, sizeof(transmit_type));
        }
        //allocate channel, configure it as slave, then open...
    }
    PRINTS("\r\n");
}


static MSG_Status
_destroy(void){
    return SUCCESS;
}

static MSG_Status
_flush(void){
    return SUCCESS;
}
static MSG_Status
_send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    if(!data){
        return FAIL;
    }
    if(dst.submodule == 0){
        MSG_ANTCommand_t * antcmd = data->buf;
        switch(antcmd->cmd){
            default:
            case ANT_PING:
                PRINTS("ANT_PING\r\n");
                break;
            case ANT_SET_ROLE:
                PRINTS("ANT_SET_ROLE\r\n");
                return _set_discovery_mode(antcmd->param.role);
            case ANT_SET_DISCOVERY_PROFILE:
                self.profile = antcmd->param.profile;
                PRINTS("claiming profile");
        }
    }
}

static MSG_Status
_init(){
    uint32_t ret;
    uint8_t network_key[8] = {0,0,0,0,0,0,0,0};
    ret = sd_ant_stack_reset();
    ret += sd_ant_network_address_set(0,network_key);
    ret += app_timer_create(&self.discovery_timeout, APP_TIMER_MODE_SINGLE_SHOT, _discovery_timeout);
    if(!ret){
        return SUCCESS;
    }else{
        return FAIL;
    }
}
MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent){
    self.parent = parent;
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
void ant_handler(ant_evt_t * p_ant_evt){
    uint8_t event = p_ant_evt->event;
    uint8_t ant_channel = p_ant_evt->channel;
    uint32_t * event_message_buffer = p_ant_evt->evt_buffer;
    PRINT_HEX(&event,1);
    PRINTS("\r\n");
    switch(event){
        case EVENT_RX_FAIL:
            //PRINTS("FRX\r\n");
            break;
        case EVENT_RX:
            PRINTS("RX\r\n");
            _handle_rx(&ant_channel,event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
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
            //PRINTS("TX\r\n");
            _handle_tx(&ant_channel,event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
            break;
        case EVENT_TRANSFER_TX_FAILED:
            break;
        case EVENT_CHANNEL_COLLISION:
            PRINTS("XX\r\n");
            break;
        case EVENT_CHANNEL_CLOSED:
            _handle_channel_closure(&ant_channel, event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
            break;
        default:
            break;
    }

}
