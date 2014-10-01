#include "message_ant.h"
#include "util.h"

static struct{
    MSG_Central_t * parent;
    MSG_Base_t base;
    MSG_ANTHandler_t * user_handler;
    hlo_ant_packet_listener message_listener;
    hlo_ant_device_t paired_devices[DEFAULT_ANT_BOND_COUNT];//TODO macro this in later
}self;
static char * name = "ANT";

//returns -1 if not found, otherwise return index
static int
_find_paired(const hlo_ant_device_t * device){
    int ret;
    for(ret = 0; ret < DEFAULT_ANT_BOND_COUNT; ret++){
        if(self.paired_devices[ret].device_number == device->device_number){
            return ret;
        }
    }
    return -1;
}
static int
_find_empty(void){
    int ret;
    for(ret = 0; ret < DEFAULT_ANT_BOND_COUNT; ret++){
        if(self.paired_devices[ret].device_number == 0){
            return ret;
        }
    }
    return -1;
}
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
                {
                    int i = _find_empty();
                    if(i >= 0){
                        self.paired_devices[(uint8_t)i] = antcmd->param.device;
                        PRINTS("Paired\r\n");
                    }
                }
                break;
        }
    }else if(dst.submodule <= DEFAULT_ANT_BOND_COUNT){
        int idx = dst.submodule - 1;
        //donp't have queue atm, dropping extra data for now
        hlo_ant_packet_send_message(&self.paired_devices[idx], data);
    }
    return SUCCESS;
}

static MSG_Status
_destroy(void){
    return SUCCESS;
}
static void _on_message(const hlo_ant_device_t * device, MSG_Data_t * message){
    MSG_Address_t default_src = {ANT, 0};
    int src_submod = _find_paired(device);
    if(src_submod >= 0){
        default_src.submodule = (uint8_t)src_submod;
        if(self.user_handler && self.user_handler->on_message)
            self.user_handler->on_message(device, default_src, message);
    }else{
        PRINTS("Unknown Source\r\n");
        if(self.user_handler && self.user_handler->on_unknown_device)
            self.user_handler->on_unknown_device(device, message);
    }
    //DEBUG print them out too
    self.parent->dispatch(default_src, (MSG_Address_t){UART, 1}, message);
}

static void _on_message_sent(const hlo_ant_device_t * device, MSG_Data_t * message){
    //get next queued tx message
    int ret = hlo_ant_disconnect(device);
    PRINTS("Message Sent!\r\n");
    if(ret < 0){
        PRINTS("error closing\r\n");
    }
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
