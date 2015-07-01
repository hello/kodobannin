#include "ant_user.h"
#include "message_uart.h"
#include "message_ble.h"
#include "morpheus_ble.h"
#include "util.h"
#include "ant_bondmgr.h"
#include "app_timer.h"

#include "platform.h"
#include "app.h"

#include "hble.h"
#include "battery.h"
#include "ant_devices.h"
#include "proto_tools.h"

static struct{
    MSG_Central_t * parent;
    volatile uint8_t pair_enable;
    app_timer_id_t commit_timer;
    ANT_BondedDevice_t staging_bond;
}self;

static void _commit_pairing(void * ctx){
    PRINTS("\r\n======\r\nCOMMIT PAIRING\r\n======\r\n");
    ANT_BondMgrCommit();
}

static void _handle_pill(const hlo_ant_device_t * id, MSG_Address_t src, MSG_Data_t * msg){
    // TODO, this shit needs to be tested on CC3200 side.
    MSG_ANT_PillData_t* pill_data = (MSG_ANT_PillData_t*)msg->buf;

    MorpheusCommand morpheus_command = {0};

    uint64_t device_id = pill_data->UUID;
    char buffer[17] = {0};
    size_t buffer_len = sizeof(buffer);

    if(!hble_uint64_to_hex_device_id(device_id, buffer, &buffer_len)){
        PRINTS("Get pill id failed.\r\n");
    }else{
        morpheus_command.deviceId.arg = buffer;

        //TODO it may be a good idea to check len from the msg
        switch(pill_data->type){
            case ANT_PILL_DATA_ENCRYPTED:
                {
                    // TODO: Jackson please test this
                    if(sizeof(buffer) > sizeof(morpheus_command.pill_data.device_id))
                    {
                        PRINTS("PLEASE REDESIGN PROTOBUF, device id tooo long\r\n");
                        APP_OK(NRF_ERROR_NO_MEM);
                    }

                    if(pill_data->payload_len > sizeof(morpheus_command.pill_data.motion_data_entrypted.bytes))
                    {
                        PRINTS("PLEASE REDESIGN PROTOBUF, payload tooo long\r\n");
                        APP_OK(NRF_ERROR_NO_MEM);
                    }


                    morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA;
                    morpheus_command.has_pill_data = true;

                    morpheus_command.pill_data.has_motion_data_entrypted = true;
                    memcpy(morpheus_command.pill_data.motion_data_entrypted.bytes, pill_data->payload, pill_data->payload_len);
                    morpheus_command.pill_data.motion_data_entrypted.size = pill_data->payload_len;

                    memcpy(morpheus_command.pill_data.device_id, buffer, sizeof(buffer));

                    morpheus_command.pill_data.has_firmware_version = true;
                    morpheus_command.pill_data.firmware_version = pill_data->version;

                        morpheus_command.pill_data.has_rssi = true;
                        morpheus_command.pill_data.rssi = id->rssi;

                    morpheus_command.pill_data.timestamp = 0;
                    PRINTS("ANT Encrypted Pill Data Received:");
                    PRINTS(morpheus_command.pill_data.device_id);
                    PRINTS("\r\n");
                }
                break;
            case ANT_PILL_HEARTBEAT:
                {
                    if(sizeof(buffer) > sizeof(morpheus_command.pill_data.device_id))
                    {
                        PRINTS("PLEASE REDESIGN PROTOBUF, device id tooo long\r\n");
                        APP_OK(NRF_ERROR_NO_MEM);
                    }

                    pill_heartbeat_t heartbeat = {0};
                    // http://dbp-consulting.com/StrictAliasing.pdf
                    memcpy(&heartbeat, pill_data->payload, sizeof(pill_heartbeat_t));
                    morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_HEARTBEAT;
                    morpheus_command.has_pill_data = true;

                    if(heartbeat.battery_level != BATTERY_INVALID_MEASUREMENT){
                        morpheus_command.pill_data.has_battery_level = true;
                        morpheus_command.pill_data.battery_level = heartbeat.battery_level;
                    }

                    morpheus_command.pill_data.has_uptime = true;
                    morpheus_command.pill_data.uptime = heartbeat.uptime_sec;

                    morpheus_command.pill_data.has_firmware_version = true;
                    morpheus_command.pill_data.firmware_version = pill_data->version;

                        morpheus_command.pill_data.has_rssi = true;
                        morpheus_command.pill_data.rssi = id->rssi;

                    memcpy(morpheus_command.pill_data.device_id, buffer, sizeof(buffer));

                    morpheus_command.pill_data.timestamp = 0;
                    PRINTS("ANT Pill Heartbeat Received.\r\n");
                }
                break;
            case ANT_PILL_SHAKING:
                PRINTS("Shaking pill: ");
                PRINT_HEX(&pill_data->UUID, sizeof(pill_data->UUID));
                PRINTS("\r\n");

                if(self.pair_enable){
                    MSG_Data_t* ble_cmd_page = MSG_Base_AllocateObjectAtomic(&pill_data->UUID, sizeof(pill_data->UUID));
                    if(ble_cmd_page){
                        self.parent->dispatch(ADDR(ANT,0), ADDR(BLE, MSG_BLE_ACK_DEVICE_ADDED), ble_cmd_page);
                        MSG_Base_ReleaseDataAtomic(ble_cmd_page);
                    }else{
                        PRINTS("No Memory!\r\n");
                    }
                }else{
                    morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_SHAKES;
                }
                break;

            default:
                break;
        }

        MSG_Data_t* proto_page = message_from_MorpheusCommand(&morpheus_command);
        if(proto_page){
            self.parent->dispatch(src, (MSG_Address_t){SSPI,1}, proto_page);
            /*
             *self.parent->dispatch(src, (MSG_Address_t){UART,1}, proto_page);
             */
            MSG_Base_ReleaseDataAtomic(proto_page);
        }else{
            PRINTS("No memory\r\n");
        }
    }
}
static void _disp_ant_id(const hlo_ant_device_t * id){
    int32_t rssi = id->rssi * -1;
    PRINTS("ANT: ");
    PRINT_HEX(&id->device_number,2);
    PRINTS(" D: ");
    PRINT_HEX(&id->device_type,1);
    PRINTS(" T: ");
    PRINT_HEX(&id->transmit_type,1);
    PRINTS(" RSSI: -");
    PRINT_DEC(&rssi,4);
    PRINTS("\r\n");
}
static void _on_message(const hlo_ant_device_t * id, MSG_Address_t src, MSG_Data_t * msg){
    if(!msg)
    {
        PRINTS("ANT Data error.\r\n");
        return;
    }
    _disp_ant_id(id);
    switch(id->device_type){
        case HLO_ANT_DEVICE_TYPE_PILL:
            _handle_pill(id, src, msg);
            break;
        default:
            PRINTS("Unknown ANT Device");
            PRINT_HEX(&id->device_type, 1);
            PRINTS("\r\n");
            break;
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
                MSG_Data_t * obj = MSG_Base_AllocateObjectAtomic(&self.staging_bond.full_uid, sizeof(self.staging_bond.full_uid));
                if(obj){
                    self.parent->dispatch( ADDR(ANT,0), ADDR(BLE, MSG_BLE_ACK_DEVICE_ADDED), obj);
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
        .on_status_update = _on_status_update,
    };
    self.parent = central;

    app_timer_create(&self.commit_timer, APP_TIMER_MODE_SINGLE_SHOT, _commit_pairing);
    return &handler;
}

inline void ANT_UserSetPairing(uint8_t enable){
    self.pair_enable = enable;
}

