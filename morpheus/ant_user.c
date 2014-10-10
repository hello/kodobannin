#include "ant_user.h"
#include "message_uart.h"
#include "message_ble.h"
#include "util.h"
#include "ant_bondmgr.h"
#include "app_timer.h"

#include "platform.h"
#include "app.h"

#include "hble.h"

static struct{
    MSG_Central_t * parent;
    volatile uint8_t pair_enable;
    volatile uint64_t dfu_pill_id;
    app_timer_id_t commit_timer;
    ANT_BondedDevice_t staging_bond;
}self;

static void _commit_pairing(void * ctx){
    PRINTS("\r\n======\r\nCOMMIT PAIRING\r\n======\r\n");
    ANT_BondMgrCommit();
}


static void _on_message(const hlo_ant_device_t * id, MSG_Address_t src, MSG_Data_t * msg){
    if(!msg)
    {
        PRINTS("Data error.\r\n");
        return;
    }

    if(SUCCESS != MSG_Base_AcquireDataAtomic(msg))
    {
        PRINTS("Internal error.\r\n");
        return;
    }

    // TODO, this shit needs to be tested on CC3200 side.
    MSG_ANT_PillData_t* pill_data = (MSG_ANT_PillData_t*)msg->buf;


    MorpheusCommand morpheus_command;
    memset(&morpheus_command, 0, sizeof(MorpheusCommand));

    morpheus_command.version = PROTOBUF_VERSION;

    uint64_t device_id = pill_data->UUID;
    char buffer[17];  // 17 = 8 * 2 + 1
    memset(buffer, 0, 17);
    size_t buffer_len = sizeof(buffer);

    if(!hble_uint64_to_hex_device_id(device_id, buffer, &buffer_len))
    {
        PRINTS("Get pill id failed.\r\n");
    }else{
        MSG_Data_t* device_id_page = MSG_Base_AllocateStringAtomic(buffer);
        if(!device_id_page)
        {
            PRINTS("No memory.\r\n");
        }else{
            morpheus_command.deviceId.arg = device_id_page;

            //TODO it may be a good idea to check len from the msg
            switch(pill_data->type){
                case ANT_PILL_DATA:
                    {
                        morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA;
                        morpheus_command.has_motionData = true;
                        morpheus_command.motionData = pill_data->payload[TF_CONDENSED_BUFFER_SIZE - 1];
                    }
                    break;
                case ANT_PILL_DATA_ENCRYPTED:
                    {
                        // TODO: Jackson please test this
                        morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA;
                        MSG_Data_t* encrypted_data = MSG_Base_AllocateDataAtomic(pill_data->payload_len);
                        memset(encrypted_data->buf, 0, encrypted_data->len);
                        memcpy(encrypted_data->buf, pill_data->payload, pill_data->payload_len);

                        morpheus_command.motionDataEntrypted.arg = encrypted_data;
                    }
                    break;
                case ANT_PILL_HEARTBEAT:
                    {
                        morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_HEARTBEAT;
                        morpheus_command.has_batteryLevel = true;
                        morpheus_command.batteryLevel = ((pill_heartbeat_t*)pill_data->payload)->battery_level;

                        morpheus_command.has_uptime = true;
                        morpheus_command.uptime = ((pill_heartbeat_t*)pill_data->payload)->uptime_sec;

                        morpheus_command.has_firmwareVersion = true;
                        morpheus_command.firmwareVersion = ((pill_heartbeat_t*)pill_data->payload)->firmware_version;
                    }
                    break;

                default:
                    break;
            }

            size_t proto_len = 0;
            if(morpheus_ble_encode_protobuf(&morpheus_command, NULL, &proto_len))
            {
                MSG_Data_t* proto_page = MSG_Base_AllocateDataAtomic(proto_len);
                if(proto_page)
                {
                    memset(proto_page->buf, 0, proto_page->len);
                    if(morpheus_ble_encode_protobuf(&morpheus_command, proto_page->buf, &proto_len))
                    {
                        self.parent->dispatch(src, (MSG_Address_t){SSPI,1}, proto_page);
                        self.parent->dispatch(src, (MSG_Address_t){UART,1}, proto_page);
                    }
                    MSG_Base_ReleaseDataAtomic(proto_page);
                }else{
                    PRINTS("No memory\r\n");
                }
            }

            MSG_Base_ReleaseDataAtomic(device_id_page);
        }

    }
    morpheus_ble_free_protobuf(&morpheus_command);
    MSG_Base_ReleaseDataAtomic(msg);
}

static void _on_unknown_device(const hlo_ant_device_t * _id, MSG_Data_t * msg){
    PRINTS("Unknown Device ID:");
    PRINT_HEX(&_id->device_number, 2);
    PRINTS("\r\n");
    MSG_ANT_PillData_t* pill_data = (MSG_ANT_PillData_t*)msg->buf;
    if(pill_data->type == ANT_PILL_SHAKING && self.pair_enable){
        self.staging_bond = (ANT_BondedDevice_t){
            .id = *_id,
            .full_uid = pill_data->UUID,
        };
        MSG_SEND_CMD(self.parent, ANT, MSG_ANTCommand_t, ANT_ADD_DEVICE, _id, sizeof(*_id));
    }
}

static void _on_status_update(const hlo_ant_device_t * id, ANT_Status_t  status){
    switch(status){
        default:
            break;
        case ANT_STATUS_DISCONNECTED:
            {
                ANT_BondedDevice_t * device = ANT_BondMgrQuery(id);
                if(device){
                    PRINTS("DEVICE REMOVED\r\n");
                    ANT_BondMgrRemove(device);
                    app_timer_start(self.commit_timer, APP_TIMER_TICKS(10000, APP_TIMER_PRESCALER), NULL);
                }
            }
            break;
        case ANT_STATUS_CONNECTED:
            if(id->device_number == self.staging_bond.id.device_number){
                PRINTS("DEVICE CONNECTED\r\n");
                PRINTS("Staging match");
                MSG_Data_t * obj = MSG_Base_AllocateDataAtomic(sizeof(MSG_BLECommand_t));
                if(obj){
                    MSG_BLECommand_t * cmd = (MSG_BLECommand_t*)obj->buf;
                    cmd->param.pill_uid = self.staging_bond.full_uid;
                    cmd->cmd = BLE_ACK_DEVICE_ADDED;
                    self.parent->dispatch( (MSG_Address_t){ANT,0}, (MSG_Address_t){BLE, 0}, obj);
                    MSG_Base_ReleaseDataAtomic(obj);
                }
                {
                    ANT_BondMgrAdd(&self.staging_bond);
                    app_timer_start(self.commit_timer, APP_TIMER_TICKS(10000, APP_TIMER_PRESCALER), NULL);
                }
            }else{
                PRINTS("Staging Mismatch");
            }
            break;
    }
}

MSG_ANTHandler_t * ANT_UserInit(MSG_Central_t * central){
    static MSG_ANTHandler_t handler = {
        .on_message = _on_message,
        .on_unknown_device = _on_unknown_device,
        .on_status_update = _on_status_update,
    };
    self.parent = central;
    self.dfu_pill_id = 0;

    app_timer_create(&self.commit_timer, APP_TIMER_MODE_SINGLE_SHOT, _commit_pairing);
    return &handler;
}

inline void ANT_UserSetPairing(uint8_t enable){
    self.pair_enable = enable;
}

inline void ant_pill_dfu_begin(uint64_t pill_id){
    self.dfu_pill_id = pill_id;
}
