#include <stdlib.h>


#include "app.h"
#include "platform.h"
#include "util.h"

#include "message_ble.h"
#include "message_ant.h"
#include "hble.h"
#include "morpheus_ble.h"

static void _reply_protobuf_error(uint32_t error_type);
static void _release_pending_resources();

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    struct pill_pairing_request pill_pairing_request;
} self;



static MSG_Status _destroy(void){
    _release_pending_resources();
    return SUCCESS;
}

static MSG_Status _flush(void){
    return SUCCESS;
}

static void _on_hlo_notify_completed(const void* data_sent, void* tag){
	MSG_Base_ReleaseDataAtomic((MSG_Data_t*)tag);
}

static void _on_hlo_notify_failed(void* tag){
    MSG_Base_ReleaseDataAtomic((MSG_Data_t*)tag);
}


static bool _encode_command_string_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    char* str = NULL;

    switch(field->tag)
    {
        case MorpheusCommand_deviceId_tag:
        {
            if(!self.pill_pairing_request.device_id)
            {
                return false;
            }

            str = self.pill_pairing_request.device_id->buf;
        }
        break;

        case MorpheusCommand_accountId_tag:
        {
            if(!self.pill_pairing_request.account_id)
            {
                return false;
            }

            str = self.pill_pairing_request.account_id->buf;
        }
        break;
    }
    
    
    if (!pb_encode_tag_for_field(stream, field))
        return false;

    return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}


static void _register_pill(){
    if(self.pill_pairing_request.account_id && self.pill_pairing_request.device_id){
        // now we have both the user's account id and pill id
        // compose the pairing request protobuf and send the shit to CC3200.
        MSG_Data_t* data_page = MSG_Base_AllocateDataAtomic(PROTOBUF_MAX_LEN);

        if(!data_page){
            PRINTS("No memory\r\n");
            _reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
        }else{
            memset(data_page->buf, 0, data_page->len);

            MorpheusCommand pairing_command;
            memset(&pairing_command, 0, sizeof(pairing_command));

            pairing_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_PILL;
            pairing_command.version = PROTOBUF_VERSION;
            pairing_command.deviceId.funcs.encode = _encode_command_string_fields;
            pairing_command.accountId.funcs.encode = _encode_command_string_fields;

            pb_ostream_t out_stream = pb_ostream_from_buffer(data_page->buf, data_page->len);
            bool status = pb_encode(&out_stream, MorpheusCommand_fields, &pairing_command);
            
            if(status)
            {
                PRINTS("Register\r\n");

                size_t protobuf_len = out_stream.bytes_written;
                MSG_Data_t* compact_page = MSG_Base_AllocateDataAtomic(protobuf_len);
                if(compact_page){
                    memcpy(compact_page->buf, data_page->buf, protobuf_len);
                    route_data_to_cc3200(compact_page);
                    MSG_Base_ReleaseDataAtomic(compact_page);
                }else{
                    _reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
                }

            }else{
                PRINTS("encode protobuf failed: ");
                PRINTS(PB_GET_ERROR(&out_stream));
                PRINTS("\r\n");
                _reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
                
            }
            MSG_Base_ReleaseDataAtomic(data_page);
        }

    }else{
        PRINTS("Invalid pairing state\r\n");
    }


    _release_pending_resources();
}

static MSG_Status _on_data_arrival(MSG_Address_t src, MSG_Address_t dst,  MSG_Data_t* data){
    if(dst.submodule == 1 && src.module == SSPI && src.submodule == 1){
    	// If we send things more than 1 packet, we might get fucked here.
    	MSG_Base_AcquireDataAtomic(data);
		// protobuf, dump the thing straight back?
		hlo_ble_notify(0xB00B, data->buf, data->len, 
            &(struct hlo_ble_operation_callbacks){_on_hlo_notify_completed, _on_hlo_notify_failed, data});
    }

    if(dst.submodule == 1 && src.module == ANT && src.submodule == 1){
        MSG_Base_AcquireDataAtomic(data);

        MSG_ANTCommand_t* ant_command = (MSG_ANTCommand_t*)data->buf;  // Shall we define a message type or move message_ant.c to corresponding app?
        
        switch(ant_command->cmd)
        {
            case ANT_ACK_DEVICE_PAIRED:  // TODO, check we don't crash here.
            {
                uint64_t* pill_id = (uint64_t*)ant_command->param.raw_data;
                size_t hex_string_len = 0;
                hble_uint64_to_hex_device_id(*pill_id, NULL, &hex_string_len);
                char hex_string[hex_string_len];
                hble_uint64_to_hex_device_id(*pill_id, hex_string, &hex_string_len);
                MSG_Data_t* pill_id_page = MSG_Base_AllocateStringAtomic(hex_string);
                self.pill_pairing_request.device_id = pill_id_page;

                _register_pill();
            }
            break;
            case ANT_ACK_DEVICE_REMOVED:
            {
                // Reply to the phone the pill in whitelist has been deleted
                // The phone should delete the pill from server.
                // DONOT keep state here.
                MorpheusCommand morpheus_command;
                morpheus_command.version = PROTOBUF_VERSION;
                morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_UNPAIR_PILL;
                morpheus_ble_reply_protobuf(&morpheus_command);
            }
            break;

            default:
            break;
        }
        
        MSG_Base_ReleaseDataAtomic(data);
    }
}

static void _reply_protobuf_error(uint32_t error_type){
    MorpheusCommand morpheus_command;
    memset(&morpheus_command, 0, sizeof(morpheus_command));
    morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_ERROR;
    morpheus_command.version = PROTOBUF_VERSION;

    morpheus_command.has_error = true;
    morpheus_command.error = error_type;

    morpheus_ble_reply_protobuf(&morpheus_command);
}

static void _release_pending_resources(){
    if(NULL != self.pill_pairing_request.account_id){
        MSG_Base_ReleaseDataAtomic(self.pill_pairing_request.account_id);
        self.pill_pairing_request.account_id = NULL;
    }

    if(NULL != self.pill_pairing_request.device_id){
        MSG_Base_ReleaseDataAtomic(self.pill_pairing_request.device_id);
        self.pill_pairing_request.device_id = NULL;
    }
}

MSG_Status send_remove_pill_notification(const char* hex_pill_id)
{
    // We should NOT keep any states here as well, so no timeout is needed and no memory leak danger.
    uint64_t pill_id = 0;
    if(!hble_hex_to_uint64_device_id(hex_pill_id, &pill_id))
    {
        _reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
        return FAIL;
    }

#ifdef ANT_ENABLE
    MSG_Data_t* command_page = MSG_Base_AllocateDataAtomic(sizeof(MSG_ANTCommand_t));
    if(!command_page)
    {
        _reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
        return FAIL;
    }
    memset(command_page->buf, 0, command_page->len);
    MSG_ANTCommand_t* ant_command = (MSG_ANTCommand_t*)command_page->buf;
    ant_command->cmd = ANT_REMOVE_DEVICE;
    memcpy(ant_command->param.raw_data, &pill_id, sizeof(pill_id));

    self.parent->dispatch((MSG_Address_t){BLE, 1},(MSG_Address_t){ANT, 1}, command_page);
    MSG_Base_ReleaseDataAtomic(command_page);
#else
    // echo test
    PRINTS("Pill id to unpair:");
    PRINT_HEX(&pill_id, sizeof(pill_id));
    PRINTS("\r\n");

    MorpheusCommand morpheus_command;
    memset(&morpheus_command, 0, sizeof(morpheus_command));
    morpheus_command.version = PROTOBUF_VERSION;
    morpheus_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_UNPAIR_PILL;
    morpheus_ble_reply_protobuf(&morpheus_command);
#endif

    return SUCCESS;

}

MSG_Status process_pending_pill_piairing_request(const char* account_id)
{
    _release_pending_resources();

    MSG_Data_t* account_id_page = MSG_Base_AllocateStringAtomic(account_id);
    if(NULL == account_id_page){
        PRINTS("Not enough memory\r\n");

        _reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
        return FAIL;
    }

    self.pill_pairing_request.account_id = account_id_page;

    // Send notification to ANT? Actually at this time ANT can just send back device id without being notified.
#ifdef ANT_ENABLE
    /*
     *ANT_ChannelID_t id = {
     *    .device_number = 0x7E1A,
     *    .device_type = HLO_ANT_DEVICE_TYPE_PILL_EVT,
     *    .transmit_type = 0
     *};
     *MSG_SEND_CMD(central, ANT, MSG_ANTCommand_t, ANT_CREATE_SESSION, &id, sizeof(id));
     */
    PRINTS("Waiting the pill to reply...\r\n");
    ANT_DISCOVERY_ACTION action = ANT_DISCOVERY_ACCEPT_NEXT_DEVICE;
    MSG_SEND_CMD(self.parent, ANT, MSG_ANTCommand_t, ANT_SET_DISCOVERY_ACTION, &action, sizeof(action));
#else
    // For echo test
    PRINTS("echo test\r\n");
    self.pill_pairing_request.device_id = MSG_Base_AllocateStringAtomic("test pill id");
    _register_pill();
#endif

    return SUCCESS;

}

MSG_Status route_data_to_cc3200(const MSG_Data_t* data){
    
    if(data){
        self.parent->dispatch((MSG_Address_t){BLE, 1},(MSG_Address_t){SSPI, 1}, data);
#ifndef ANT_ENABLE
        // Echo test
        if(SUCCESS == MSG_Base_AcquireDataAtomic(data))
        {
            hlo_ble_notify(0xB00B, data->buf, data->len, 
                        &(struct hlo_ble_operation_callbacks){_on_hlo_notify_completed, _on_hlo_notify_failed, data});
        }
#endif
        
        return SUCCESS;
    }else{
        return FAIL;
    }
}

static MSG_Status _init(){
    self.pill_pairing_request = (struct pill_pairing_request){};

    // Tests
    // self.pill_pairing_request.device_id = MSG_Base_AllocateStringAtomic("test pill id");
    return SUCCESS;
}

MSG_Base_t* MSG_BLE_Base(MSG_Central_t* parent){
    self.parent = parent;
    
    self.base.init =  _init;
    self.base.flush = _flush;
    self.base.send = _on_data_arrival;
    self.base.destroy = _destroy;
    self.base.type = BLE;
    self.base.typestr = "BLE";
    
    return &self.base;

}

