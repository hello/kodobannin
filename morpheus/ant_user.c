#include "ant_user.h"
#include "message_uart.h"

static struct{
    MSG_Central_t * parent;
}self;
static void _on_message(const ANT_ChannelID_t * id, MSG_Address_t src, MSG_Data_t * msg){
    self.parent->dispatch(src, (MSG_Address_t){UART, 1}, msg);
    self.parent->dispatch(src, (MSG_Address_t){SSPI,1}, msg);
}

MSG_ANTHandler_t * morpheus_ant_user(MSG_Central_t * central){
    static MSG_ANTHandler_t handler = {
        .on_message = _on_message
    };
    self.parent = central;
    return &handler;
}
