#include <ant_interface.h>
#include "message_ant.h"

typedef enum{
    UNUSED_CHANNEL = 0,
    CONTROL_CHANNEL,
    SINGLE_CHANNEL,
    SHARED_CHANNEL,
    UNKNOWN_CHANNEL
}ANT_ChannelType;

typedef struct{
    ANT_ChannelType type;

}ANT_Channel_t;

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    ANT_Channel_t channels[8];//only 8 channels on 51422
}self;
static char * name = "ANT";

static MSG_Status
_set_discovery_mode(uint8_t role){
    switch(role){
        case 0:
            //central mode
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
            case ANT_Set_Role:
                return _set_discovery_mode(cmd->param.role);
            case ANT_Unpair_Channel:
                break;
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
