#include "ant_user.h"
#include "message_uart.h"
#include "message_ble.h"
#include "morpheus_ble.h"
#include "util.h"
#include "app_timer.h"

#include "platform.h"
#include "app.h"

#include "hble.h"
#include "battery.h"
#include "ant_devices.h"

static struct{
    MSG_Central_t * parent;
    volatile uint8_t pair_enable;
    volatile uint64_t dfu_pill_id;
}self;

static int _copy_pill_meta_data(MorpheusCommand * c, MSG_ANT_PillData_t * pill_data, const hlo_ant_device_t * id, char * device_id){
    memcpy(c->pill_data.device_id, device_id, sizeof(c->pill_data.device_id));

    c->pill_data.has_firmware_version = true;
    c->pill_data.firmware_version = pill_data->version;

    c->pill_data.has_rssi = true;
    c->pill_data.rssi = id->rssi;

    c->pill_data.timestamp = 0;
    return 0;
}
static int _copy_encrypted_data(MorpheusCommand * c, MorpheusCommand_CommandType type, MSG_ANT_PillData_t * pill_data){
    c->type = type;
    c->has_pill_data = true;

    c->pill_data.has_motion_data_entrypted = true;
    memcpy(c->pill_data.motion_data_entrypted.bytes, pill_data->payload, pill_data->payload_len);
    c->pill_data.motion_data_entrypted.size = pill_data->payload_len;

    return 0;
}
static void _handle_pill(const hlo_ant_device_t * id, MSG_Address_t src, MSG_Data_t * msg){
    // TODO, this shit needs to be tested on CC3200 side.
    MSG_ANT_PillData_t* pill_data = (MSG_ANT_PillData_t*)msg->buf;

    MorpheusCommand morpheus_command;
    memset(&morpheus_command, 0, sizeof(MorpheusCommand));

    uint64_t device_id = pill_data->UUID;
    char buffer[sizeof(morpheus_command.pill_data.device_id)] = {0};
    size_t buffer_len = sizeof(buffer);

    if(!hble_uint64_to_hex_device_id(device_id, buffer, &buffer_len)){
        PRINTS("Get pill id failed.\r\n");
    }else{
        if( MSG_Base_FreeCount() < configLOW_MEM )
        {
            PRINTS("Low memory, pill data dropped.\r\n");
        }else{
            MSG_Data_t* device_id_page = MSG_Base_AllocateStringAtomic(buffer);
            
            if( !device_id_page ) {
                PRINTS("Failed to allocate pill data page.\r\n");
            } else {
                morpheus_command.deviceId.arg = device_id_page;

                //TODO it may be a good idea to check len from the msg
                switch(pill_data->type){
                    case ANT_PILL_PROX_ENCRYPTED:
                        {
                            if(pill_data->payload_len > sizeof(morpheus_command.pill_data.motion_data_entrypted.bytes))
                            {
                                PRINTS("PLEASE REDESIGN PROTOBUF, payload tooo long\r\n");
                                APP_OK(NRF_ERROR_NO_MEM);
                            }

                            _copy_pill_meta_data(&morpheus_command, pill_data, id, buffer);
                            _copy_encrypted_data(&morpheus_command, MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_PROX_DATA, pill_data);

                            PRINTS("ANT Encrypted Pill Prox Received:");
                            PRINTS(morpheus_command.pill_data.device_id);
                            PRINTS("\r\n");
                        }
                        break;
                    case ANT_PILL_DATA_ENCRYPTED:
                        {
                            if(pill_data->payload_len > sizeof(morpheus_command.pill_data.motion_data_entrypted.bytes))
                            {
                                PRINTS("PLEASE REDESIGN PROTOBUF, payload tooo long\r\n");
                                APP_OK(NRF_ERROR_NO_MEM);
                            }

                            _copy_pill_meta_data(&morpheus_command, pill_data, id, buffer);
                            _copy_encrypted_data(&morpheus_command, MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA, pill_data);

                            PRINTS("ANT Encrypted Pill Data Received:");
                            PRINTS(morpheus_command.pill_data.device_id);
                            PRINTS("\r\n");
                        }
                        break;
                    case ANT_PILL_HEARTBEAT:
                        {
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

                            _copy_pill_meta_data(&morpheus_command, pill_data, id, buffer);

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
                            /*
                             *self.parent->dispatch(src, (MSG_Address_t){UART,1}, proto_page);
                             */
                        }
                        MSG_Base_ReleaseDataAtomic(proto_page);
                    }else{
                        PRINTS("No memory\r\n");
                    }
                }

                MSG_Base_ReleaseDataAtomic(device_id_page);
            }
        }
    }
    morpheus_ble_free_protobuf(&morpheus_command);
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
    PRINT_DEC(&rssi);
    PRINTS("\r\n");
}
static int8_t rssi_limit;

void ant_set_rssi_limit(int8_t limit){
    rssi_limit = limit;
}
static void _on_message(const hlo_ant_device_t * id, MSG_Address_t src, MSG_Data_t * msg){
    if(!msg)
    {
        PRINTS("ANT Data error.\r\n");
        return;
    }
    if(rssi_limit != 0 && id->rssi < rssi_limit){
        PRINTS("dropping due to rssi: ");
        _disp_ant_id(id);
        PRINTS("\r\n");
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

}

MSG_ANTHandler_t * ANT_UserInit(MSG_Central_t * central){
    static MSG_ANTHandler_t handler = {
        .on_message = _on_message,
        .on_status_update = _on_status_update,
    };
    self.parent = central;
    self.dfu_pill_id = 0;
    rssi_limit = -58;

    return &handler;
}

inline void ANT_UserSetPairing(uint8_t enable){
    self.pair_enable = enable;
}

inline void ant_pill_dfu_begin(uint64_t pill_id){
    self.dfu_pill_id = pill_id;
}
