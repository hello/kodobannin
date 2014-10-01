#include "ant_user.h"

static struct{
    MSG_Central_t * parent;
}self;

static void _on_message(const hlo_ant_device_t * id, MSG_Address_t src, MSG_Data_t * msg){
}

static void _on_unknown_device(const hlo_ant_device_t * id, MSG_Data_t * msg){
}

static void _on_status_update(const hlo_ant_device_t * id, ANT_Status_t  status){
}

MSG_ANTHandler_t * ANT_UserInit(MSG_Central_t * central){
    static MSG_ANTHandler_t handler = {
        .on_message = _on_message,
        .on_unknown_device = _on_unknown_device,
        .on_status_update = _on_status_update,
    };
    self.parent = central;
    return &handler;
}
