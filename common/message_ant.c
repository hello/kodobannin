#include <ant_interface.h>
#include <app_error.h>
#include <ant_parameters.h>
#include "message_ant.h"
#include "util.h"

#define NUM_ANT_CHANNELS 8
#define ANT_EVENT_MSG_BUFFER_MIN_SIZE 32u  /**< Minimum size of an ANT event message buffer. */

typedef struct{

}ANT_Channel_t;

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    ANT_Channel_t channels[NUM_ANT_CHANNELS];//only 8 channels on 51422
}self;
static char * name = "ANT";

static MSG_Status
_destroy_channel(uint8_t channel){
    uint32_t ret = 0;
    if(channel < NUM_ANT_CHANNELS){
        ret |= sd_ant_channel_unassign(channel);
        ret |= sd_ant_channel_close(channel);
    }
    if(!ret){
        return SUCCESS;
    }else{
        return FAIL;
    }
}
static MSG_Status
_set_discovery_mode(uint8_t role){
    _destroy_channel(0);
    switch(role){
        case 0:
            //central mode
            APP_OK(sd_ant_channel_assign(0, CHANNEL_TYPE_SHARED_SLAVE, 0, 0));
            APP_OK(sd_ant_channel_radio_freq_set(0, 66));
            APP_OK(sd_ant_channel_period_set(0,3277));
            APP_OK(sd_ant_channel_id_set(0,
                        0,//Two byte deive number
                        1,//1 byte device type
                        5));//transmit type
            //APP_OK(sd_ant_rx_scan_mode_start(1));
            APP_OK(sd_ant_channel_open(0));
            break;
        case 1:
            //peripheral mode
            break;
        default:
            return FAIL;
    }
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
            PRINTS("A");
            switch(event){
                case EVENT_RX:
                    PRINTS("ARX\r\n");
                    break;
                case EVENT_RX_SEARCH_TIMEOUT:
                    PRINTS("ATO\r\n");
                    break;
                case EVENT_RX_FAIL_GO_TO_SEARCH:
                    PRINTS("AFTS\r\n");
                    break;
                case EVENT_TRANSFER_RX_FAILED:
                    break;
                case EVENT_TX:
                    break;
                case EVENT_TRANSFER_TX_FAILED:
                    break;
                case EVENT_CHANNEL_COLLISION:
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
