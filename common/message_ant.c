#include <ant_interface.h>
#include <app_error.h>
#include <ant_parameters.h>
#include "message_ant.h"
#include "util.h"

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

typedef struct{
    uint8_t status;
    //shared by both master and slave
    uint8_t channel_type;//MASTER SLAVE ETC
    uint8_t channel_frequency;
    uint8_t network_key;
    uint16_t channel_period;
    //below are set by master only
    uint8_t master_transmit_type;
    uint8_t master_device_type;
    uint16_t master_device_number;
}ANT_Channel_t;

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    ANT_Channel_t channels[NUM_ANT_CHANNELS];//only 8 channels on 51422
}self;
static char * name = "ANT";
#define CHANNEL_NUM_CHECK(ch) (ch < NUM_ANT_CHANNELS)

static uint8_t
_update_channel_status(uint8_t channel){
    uint8_t ret = 0;
    if(!sd_ant_channel_status_get(channel, &ret)){
        self.channels[channel].status = ret;
    }
    return ret;
}
static MSG_Status
_destroy_channel(uint8_t channel){
    uint32_t ret = 0;
    if(_update_channel_status(channel)){
        ret |= sd_ant_channel_unassign(channel);
        ret |= sd_ant_channel_close(channel);
        _update_channel_status(channel);
    }
    if(!ret){
        return SUCCESS;
    }else{
        return FAIL;
    }
}
//this closes the channel
static MSG_Status
_reconfig_channel(uint8_t channel, const ANT_Channel_t * config){
    MSG_Status ret;
    ret = _destroy_channel(channel);
    self.channels[channel] = *config;
    return ret;

}
static MSG_Status
_open_channel(uint8_t channel){
    uint32_t err = 0;
    ANT_Channel_t * cfg = &self.channels[channel];
    if(!_update_channel_status(channel)){
        err += sd_ant_channel_assign(channel,cfg->channel_type, cfg->network_key, 0);
        err += sd_ant_channel_radio_freq_set(channel, cfg->channel_frequency);
        err += sd_ant_channel_period_set(channel, cfg->channel_period);
        err += sd_ant_channel_id_set(channel, cfg->master_device_number, cfg->master_device_type, cfg->master_transmit_type);
        err += sd_ant_channel_open(channel);
    }else{
        return FAIL;
    }
    if(!err){
        return SUCCESS;
    }else{
        return FAIL;
    }
}
static MSG_Status
_set_discovery_mode(uint8_t role){
    ANT_Channel_t ch = {
        .channel_type = CHANNEL_TYPE_SHARED_SLAVE,
        .channel_frequency = 66,
        .network_key = 1,
        .channel_period = 32768,
        .master_transmit_type = 0,
        .master_device_type = 0,
        .master_device_number = 0
    };
    switch(role){
        case 0:
            //central mode
            _reconfig_channel(0, &ch);
            return _open_channel(0);
        case 1:
            //peripheral mode
            ch.master_transmit_type = 5;
            ch.master_device_type = 1;
            ch.master_device_number = 34;//set to actual number xor from devid
            ch.channel_type = CHANNEL_TYPE_SHARED_MASTER;
            _reconfig_channel(0, &ch);
            return _open_channel(0);
            break;
        default:
            return _destroy_channel(0);
    }
}
static void
_handle_channel_closure(uint8_t * channel, uint8_t * buf, uint8_t buf_size){
    _open_channel(*channel);
}

static void
_handle_rx(uint8_t * channel, uint8_t * buf, uint8_t buf_size){
    ANT_MESSAGE * msg = (ANT_MESSAGE*)buf;
    PRINT_HEX(msg->ANT_MESSAGE_aucPayload,ANT_STANDARD_DATA_PAYLOAD_SIZE);
    if(*channel == 0){
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
        }
    }
}

static MSG_Status
_init(){
    uint32_t ret;
    ret = sd_ant_stack_reset();
    if(!ret){
        return SUCCESS;
    }else{
        return FAIL;
    }
}
void ant_handler(void* event_data, uint16_t event_size){
    uint8_t event;
    uint8_t ant_channel;  
    uint8_t event_message_buffer[ANT_EVENT_MSG_BUFFER_MIN_SIZE]; 
    uint32_t err_code;
    do{
        err_code = sd_ant_event_get(&ant_channel, &event, event_message_buffer);
        if (err_code == NRF_SUCCESS)
        {
            PRINT_HEX(&event, sizeof(event));
            switch(event){
                case EVENT_RX_FAIL:
                    PRINTS("FRX\r\n");
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
                    break;
                case EVENT_TRANSFER_TX_FAILED:
                    break;
                case EVENT_CHANNEL_COLLISION:
                    break;
                case EVENT_CHANNEL_CLOSED:
                    _handle_channel_closure(&ant_channel, event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
                    break;
                default:
                    break;
            }
        }            
    }while (err_code == NRF_SUCCESS);
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
