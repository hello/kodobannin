#include "ant_user.h"
#include "message_uart.h"
#include "message_ble.h"
#include "util.h"

static struct{
    MSG_Central_t * parent;
    volatile uint8_t pair_enable;
}self;

static void _on_message(const ANT_ChannelID_t * id, MSG_Address_t src, MSG_Data_t * msg){
    self.parent->dispatch(src, (MSG_Address_t){UART,1}, msg);
    self.parent->dispatch(src, (MSG_Address_t){SSPI,1}, msg);
}

static void _on_unknown_device(const ANT_ChannelID_t * id){
    if(self.pair_enable){
        MSG_SEND_CMD(self.parent, ANT, MSG_ANTCommand_t, ANT_CREATE_SESSION, id, sizeof(*id));
        self.pair_enable = 0;
    }
}

static void _on_control_message(const ANT_ChannelID_t * id, MSG_Address_t src, uint8_t control_type, const uint8_t * control_payload){
    
}
static void _on_status_update(const ANT_ChannelID_t * id, ANT_Status_t  status){
    switch(status){
        default:
            break;
        case ANT_STATUS_DISCONNECTED:
            break;
        case ANT_STATUS_CONNECTED:
            PRINTS("DEVICE CONNECTED\r\n");
            {
                MSG_Data_t * obj = MSG_Base_AllocateDataAtomic(sizeof(MSG_BLECommand_t));
                if(obj){
                    MSG_BLECommand_t * cmd = (MSG_BLECommand_t*)obj->buf;
                    cmd->param.pill_uid = 0x12345678;
                    cmd->cmd = BLE_ACK_DEVICE_ADDED;
                    self.parent->dispatch( (MSG_Address_t){0,0}, (MSG_Address_t){BLE, 0}, obj);
                    MSG_Base_ReleaseDataAtomic(obj);
                }
            }
            break;
    }
}

MSG_ANTHandler_t * ANT_UserInit(MSG_Central_t * central){
    static MSG_ANTHandler_t handler = {
        .on_message = _on_message,
        .on_unknown_device = _on_unknown_device,
        .on_control_message = _on_control_message,
        .on_status_update = _on_status_update,
    };
    self.parent = central;
    ANT_UserPairNextDevice();
    return &handler;
}

void ANT_UserPairNextDevice(void){
    self.pair_enable = 1;
}
