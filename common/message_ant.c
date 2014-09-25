#include "message_ant.h"
#include "util.h"

static struct{
    MSG_Central_t * parent;
    MSG_Base_t base;
    MSG_ANTHandler_t * user_handler;
    hlo_ant_packet_listener message_listener;
}self;
static char * name = "ANT";

static MSG_Status
_init(void){
    return SUCCESS;
}
static MSG_Status
_flush(void){
    return SUCCESS;
}
static MSG_Status
_send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    if(dst.submodule == 0){
        MSG_ANTCommand_t * antcmd = (MSG_ANTCommand_t*)data->buf;
        switch(antcmd->cmd){
            default:
                break;
            case ANT_SET_ROLE:
                hlo_ant_init(antcmd->param.role, hlo_ant_packet_init(&self.message_listener));
                break;

            case ANT_REMOVE_DEVICE:
                break;
            case ANT_ADD_DEVICE:
                break;
        }
    }
    return SUCCESS;
}

static MSG_Status
_destroy(void){
    return SUCCESS;
}
static void _on_message(const hlo_ant_device_t * device, MSG_Data_t * message){
    //dump message into work queue
    PRINTS("Seen Message\r\n");
    self.parent->dispatch((MSG_Address_t){0,0}, (MSG_Address_t){UART, 1}, message);
}

static void _on_message_sent(const hlo_ant_device_t * device, MSG_Data_t * message){
    //get next queued tx message
}

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent, const MSG_ANTHandler_t * handler){
    self.parent = parent;
    self.user_handler = handler;
    self.message_listener.on_message = _on_message;
    self.message_listener.on_message_sent = _on_message_sent;
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
