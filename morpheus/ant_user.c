#include "ant_user.h"
#include "message_uart.h"
#include "message_ble.h"
#include "util.h"
#include "ant_bondmgr.h"
#include "app_timer.h"
#include "app.h"

static struct{
    MSG_Central_t * parent;
    volatile uint8_t pair_enable;
    app_timer_id_t commit_timer;
}self;

static void _commit_pairing(void * ctx){
    PRINTS("\r\n======\r\nCOMMIT PAIRING\r\n======\r\n");
}
static void _on_message(const ANT_ChannelID_t * id, MSG_Address_t src, MSG_Data_t * msg){
    self.parent->dispatch(src, (MSG_Address_t){UART,1}, msg);
    self.parent->dispatch(src, (MSG_Address_t){SSPI,1}, msg);
}

static void _on_unknown_device(const ANT_ChannelID_t * id){
    if(self.pair_enable){
        MSG_SEND_CMD(self.parent, ANT, MSG_ANTCommand_t, ANT_CREATE_SESSION, id, sizeof(*id));
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
                    cmd->param.pill_uid = 0x12345678;  // TODO: change to real production code?
                    cmd->cmd = BLE_ACK_DEVICE_ADDED;
                    self.parent->dispatch( (MSG_Address_t){0,0}, (MSG_Address_t){BLE, 0}, obj);
                    MSG_Base_ReleaseDataAtomic(obj);
                }
                {
                    ANT_BondedDevice_t dev = {
                        .id = *id,
                        .full_uid = id->device_number,
                    };
                    ANT_BondMgrAdd(&dev);
                }
                app_timer_start(self.commit_timer, APP_TIMER_TICKS(10000, APP_TIMER_PRESCALER), NULL);
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
    app_timer_create(&self.commit_timer, APP_TIMER_MODE_SINGLE_SHOT, _commit_pairing);
    return &handler;
}

void ANT_UserSetPairing(uint8_t enable){
    self.pair_enable = enable;
}
