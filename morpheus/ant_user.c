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
}self;

static void _commit_pairing(void * ctx){
    PRINTS("\r\n======\r\nCOMMIT PAIRING\r\n======\r\n");
}

static bool _encode_pill_command_string_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    char* str = NULL;

    switch(field->tag)
    {
        case MorpheusCommand_deviceId_tag:
        {
            // This would be the pill id in uint64_t
            uint64_t* device_id_ptr = *arg;
            if(!device_id_ptr)
            {
                return false;
            }

            char buffer[17];
            str = buffer;

            memset(buffer, 0, 17);
            size_t buffer_len = sizeof(buffer);

            if(!hble_uint64_to_hex_device_id(*device_id_ptr, buffer, &buffer_len))
            {
                PRINTS("Get pill id failed.\r\n");
            }
        }
        break;
    }
    
    
    if (!pb_encode_tag_for_field(stream, field))
        return false;

    return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}


static void _on_message(const hlo_ant_device_t * id, MSG_Address_t src, MSG_Data_t * msg){
    if(!msg)
    {
        PRINTS("Data error.\r\n");
        return;
    }


    // TODO, this shit needs to be tested on CC3200 side.
    MSG_ANT_PillData_t* pill_data = (MSG_ANT_PillData_t*)msg->buf;

    MorpheusCommand morpheus_command;
    memset(&morpheus_command, 0, sizeof(MorpheusCommand));

    morpheus_command.version = PROTOBUF_VERSION;
    morpheus_command.deviceId.funcs.encode = _encode_pill_command_string_fields;
    morpheus_command.deviceId.arg = &pill_data->UUID;

    //TODO it may be a good idea to check len from the msg
    switch(pill_data->type){
        case ANT_PILL_DATA:
            {
                morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA;
                morpheus_command.has_motionData = true;
                morpheus_command.motionData = pill_data->payload[TF_CONDENSED_BUFFER_SIZE - 1];
            }
            break;
        case ANT_PILL_HEARTBEAT:
            {
                morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_HEARTBEAT;
                morpheus_command.has_batteryLevel = true;
                morpheus_command.batteryLevel = ((pill_heartbeat_t*)pill_data->payload)->battery_level;

                morpheus_command.has_uptime = true;
                morpheus_command.uptime = ((pill_heartbeat_t*)pill_data->payload)->uptime_sec;
            }
            break;
        default:
            //do not send unknown packet to CC
            return;
    }

    MSG_Data_t* proto_page = MSG_Base_AllocateDataAtomic(PROTOBUF_MAX_LEN);

    if(!proto_page){
        PRINTS("No memory for convert protobuf.\r\n");
    }else{

        pb_ostream_t out_stream = pb_ostream_from_buffer(proto_page->buf, proto_page->len);
        bool status = pb_encode(&out_stream, MorpheusCommand_fields, &morpheus_command);
        if(!status)
        {
            PRINTS("Encoding protobuf failed, error: ");
            PRINTS(PB_GET_ERROR(&out_stream));
            PRINTS("\r\n");

        }else{
            self.parent->dispatch(src, (MSG_Address_t){SSPI,1}, proto_page);
            self.parent->dispatch(src, (MSG_Address_t){UART,1}, proto_page);
        }

        MSG_Base_ReleaseDataAtomic(proto_page);
    }
}

static void _on_unknown_device(const hlo_ant_device_t * id, MSG_Data_t * msg){
    //MSG_SEND_CMD(self.parent, ANT, MSG_ANTCommand_t, ANT_ADD_DEVICE, id, sizeof(*id));
}

static void _on_status_update(const hlo_ant_device_t * id, ANT_Status_t  status){
    switch(status){
        default:
            break;
        case ANT_STATUS_DISCONNECTED:
            break;
        case ANT_STATUS_CONNECTED:
            PRINTS("DEVICE CONNECTED\r\n");

            if(self.pair_enable)
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

                // Send a message back to pill
                // Notify it is paired and tell it flash the LED.

                // Timer?
                app_timer_start(self.commit_timer, APP_TIMER_TICKS(10000, APP_TIMER_PRESCALER), NULL);
            }

            if(self.dfu_pill_id)
            {
                // Send DFU message to pill.
                // tell the pill to erase it's setting flash and enter DFU mode.
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
