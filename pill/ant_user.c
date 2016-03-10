#include "ant_user.h"

static struct{
    MSG_Central_t * parent;
}self;

static void _on_message(const hlo_ant_device_t * id, MSG_Data_t * msg){
}
static MSG_Data_t * _on_connection(const hlo_ant_device_t * id){
    return NULL;
}


MSG_ANTHandler_t * ANT_UserInit(MSG_Central_t * central){
    static MSG_ANTHandler_t handler = {
        .on_message = _on_message,
        .on_connection = _on_connection,
    };
    self.parent = central;
    return &handler;
}
