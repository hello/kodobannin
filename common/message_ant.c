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


static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    ANT_Channel_t channels[NUM_ANT_CHANNELS];//only 8 channels on 51422
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
        .channel_type = CHANNEL_TYPE_SHARED_SLAVE,
        .network = 1};
    _destroy_channel(0);
    switch(role){
        case 0:
            //central mode
            _configure_channel(0, &phy);
            _connect(0, &id);
            break;
        case 1:
            //peripheral mode
            //configure shit here
            _configure_channel(0, &phy);
            _connect(0, &id);
            break;
        default:
            break;
    }
}
static void
_handle_channel_closure(uint8_t * channel, uint8_t * buf, uint8_t buf_size){
    PRINTS("Re-opening Channel ");
    PRINT_HEX(channel, 1);
    PRINTS("\r\n");
}

static void
_handle_rx(uint8_t * channel, uint8_t * buf, uint8_t buf_size){
    ANT_MESSAGE * msg = (ANT_MESSAGE*)buf;
    PRINT_HEX(msg->ANT_MESSAGE_aucPayload,ANT_STANDARD_DATA_PAYLOAD_SIZE);
    if(*channel == 0){
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
