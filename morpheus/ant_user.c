#include "ant_user.h"
#include "message_uart.h"

static struct{
    MSG_Central_t * parent;
}self;

static void _on_message(const ANT_ChannelID_t * id, MSG_Address_t src, MSG_Data_t * msg){
    self.parent->dispatch(src, (MSG_Address_t){UART, 1}, msg);
    self.parent->dispatch(src, (MSG_Address_t){SSPI,1}, msg);
}

static void _on_unknown_device(const ANT_ChannelID_t * id){
    MSG_SEND_CMD(self.parent, ANT, MSG_ANTCommand_t, ANT_CREATE_SESSION, id, sizeof(*id));
}

static void _on_control_message(const ANT_ChannelID_t * id, MSG_Address_t src, uint8_t control_type, const uint8_t * control_payload){
    
}

MSG_ANTHandler_t * morpheus_ant_user(MSG_Central_t * central){
    static MSG_ANTHandler_t handler = {
        .on_message = _on_message,
        .on_unknown_device = _on_unknown_device,
        .on_control_message = _on_control_message
    };
    self.parent = central;
    return &handler;
}
