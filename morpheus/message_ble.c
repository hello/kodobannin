/* Application layer for Morpheus BLE */

#include <stdlib.h>
#include <app_timer.h>

#include "app.h"
#include "platform.h"
#include "util.h"
#include "ble_bondmngr_cfg.h"

#include "message_ble.h"
#include "hble.h"
#include "morpheus_ble.h"

#ifdef ANT_STACK_SUPPORT_REQD
#include "message_ant.h"
#include "ant_user.h"
#endif

static void _release_pending_resources();

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    struct pill_pairing_request pill_pairing_request;
    app_timer_id_t timer_id;
} self;



static MSG_Status _destroy(void){
    _release_pending_resources();
    return SUCCESS;
}

static MSG_Status _flush(void){
    return SUCCESS;
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
            morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
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
                    message_ble_route_data_to_cc3200(compact_page);
                    MSG_Base_ReleaseDataAtomic(compact_page);
                }else{
                    morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
                }

            }else{
                PRINTS("encode protobuf failed: ");
                PRINTS(PB_GET_ERROR(&out_stream));
                PRINTS("\r\n");
                morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
                
            }
            MSG_Base_ReleaseDataAtomic(data_page);
        }

    }else{
        PRINTS("Invalid pairing state\r\n");
    }

    _release_pending_resources();
}

static MSG_Status _on_data_arrival(MSG_Address_t src, MSG_Address_t dst,  MSG_Data_t* data){
    if(!data){
        return FAIL;
    }
    MSG_BLECommand_t * cmd = (MSG_BLECommand_t *)data->buf;
    if(dst.submodule == 0){
        switch(cmd->cmd){
            default:
            case BLE_PING:
                break;
            case BLE_ACK_DEVICE_REMOVED:
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
            case BLE_ACK_DEVICE_ADDED:
                {
                    app_timer_stop(self.timer_id);
                    ANT_UserSetPairing(0);
                    uint64_t* pill_id = &(cmd->param.pill_uid);
                    size_t hex_string_len = 0;
                    hble_uint64_to_hex_device_id(*pill_id, NULL, &hex_string_len);
                    char hex_string[hex_string_len];
                    hble_uint64_to_hex_device_id(*pill_id, hex_string, &hex_string_len);
                    if(self.pill_pairing_request.device_id){
                        MSG_Base_ReleaseDataAtomic(self.pill_pairing_request.device_id);
                        self.pill_pairing_request.device_id = NULL;
                    }
                    MSG_Data_t* pill_id_page = MSG_Base_AllocateStringAtomic(hex_string);
                    if(pill_id_page){
                        self.pill_pairing_request.device_id = pill_id_page;
                        _register_pill();
                    }else{
                        morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
                    }
                }
                break;
        }

    }else{
        MSG_Base_AcquireDataAtomic(data);
        // protobuf, dump the thing straight back?
        hlo_ble_notify(0xB00B, data->buf, data->len,
                &(struct hlo_ble_operation_callbacks){morpheus_ble_on_notify_completed, morpheus_ble_on_notify_failed, data});
    }
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

MSG_Status message_ble_remove_pill(const MSG_Data_t* pill_id_page)
{
    if(SUCCESS != MSG_Base_AcquireDataAtomic(pill_id_page)){
        morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
        return FAIL;
    }

    const char* hex_pill_id = pill_id_page->buf;
    // We should NOT keep any states here as well, so no timeout is needed and no memory leak danger.
    uint64_t pill_id = 0;
    if(!hble_hex_to_uint64_device_id(hex_pill_id, &pill_id))
    {
        morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
    }else{

#ifdef ANT_ENABLE
        MSG_Data_t* command_page = MSG_Base_AllocateDataAtomic(sizeof(MSG_ANTCommand_t));
        if(!command_page)
        {
            morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
            return FAIL;
        }else{
/*
 *            memset(command_page->buf, 0, command_page->len);
 *            MSG_ANTCommand_t* ant_command = (MSG_ANTCommand_t*)command_page->buf;
 *            ant_command->cmd = ANT_REMOVE_DEVICE;
 *            memcpy(ant_command->param.raw_data, &pill_id, sizeof(pill_id));
 *
 *            self.parent->dispatch((MSG_Address_t){BLE, 1},(MSG_Address_t){ANT, 1}, command_page);
 *            MSG_Base_ReleaseDataAtomic(command_page);
 */
        }
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
    }

    MSG_Base_ReleaseDataAtomic(pill_id_page);


    return SUCCESS;

}


static void _pill_pairing_time_out(void* context)
{
    _release_pending_resources();
    morpheus_ble_reply_protobuf_error(ErrorType_TIME_OUT);
    ANT_UserSetPairing(0);
    PRINTS("Pill pairing time out.\r\n");
}



MSG_Status message_ble_pill_pairing_begin(const MSG_Data_t* account_id_page)
{
    _release_pending_resources();

    MSG_Base_AcquireDataAtomic(account_id_page);
    self.pill_pairing_request.account_id = account_id_page;

    // Send notification to ANT? Actually at this time ANT can just send back device id without being notified.
#ifdef ANT_ENABLE
    PRINTS("Waiting the pill to reply...\r\n");
    if(!self.timer_id)
    {
        // One minute timeout
        if(NRF_SUCCESS == app_timer_start(self.timer_id, APP_PILL_PAIRING_TIMEOUT_INTERVAL, NULL))
        {
            ANT_UserSetPairing(1);
        }else{
            morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
            _release_pending_resources();
        }
        
    }else{
        morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
        _release_pending_resources();
    }
	
#else
    // For echo test
    PRINTS("echo test\r\n");
    self.pill_pairing_request.device_id = MSG_Base_AllocateStringAtomic("test pill id");
    _register_pill();

#endif

    return SUCCESS;

}

MSG_Status message_ble_route_data_to_cc3200(const MSG_Data_t* data){
    
    if(SUCCESS == MSG_Base_AcquireDataAtomic(data)){
        self.parent->dispatch((MSG_Address_t){BLE, 1},(MSG_Address_t){SSPI, 1}, data);

#ifndef ANT_ENABLE
        // Echo test
        hlo_ble_notify(0xB00B, data->buf, data->len, 
                    &(struct hlo_ble_operation_callbacks){
                        morpheus_ble_on_notify_completed, 
                        morpheus_ble_on_notify_failed, 
                        data
                    });
#endif
        MSG_Base_ReleaseDataAtomic(data);
        
        return SUCCESS;
    }else{
        return FAIL;
    }
}

static MSG_Status _init(){
    self.pill_pairing_request = (struct pill_pairing_request){};
    app_timer_create(&self.timer_id, APP_TIMER_MODE_SINGLE_SHOT, _pill_pairing_time_out);

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

void message_ble_reset()
{
    if(self.timer_id)
    {
        app_timer_stop(self.timer_id);
    }
#ifdef ANT_ENABLE
    ANT_UserSetPairing(0);
#endif
    _release_pending_resources();

}


static void _erase_bonded_users(){
    PRINTS("Trying to erase paired centrals....\r\n");

    hble_erase_other_bonded_central();

    MorpheusCommand command;
    memset(&command, 0, sizeof(command));
    
    command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_EREASE_PAIRED_PHONE;
    command.version = PROTOBUF_VERSION;
    morpheus_ble_reply_protobuf(&command);
}



static void _led_pairing_mode(void)
{
    // TODO: Notify the led
}


static void _morpheus_switch_mode(bool is_pairing_mode)
{
    if(is_pairing_mode)
    {
        uint16_t paired_users_count = BLE_BONDMNGR_MAX_BONDED_CENTRALS;
        APP_OK(ble_bondmngr_central_ids_get(NULL, &paired_users_count));

        if(paired_users_count == BLE_BONDMNGR_MAX_BONDED_CENTRALS)
        {
            PRINTS("Pairing database full.\r\n");
            morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_DATABASE_FULL);
            return;
        }
    }


    hble_set_advertising_mode(is_pairing_mode);
    // reply to 0xB00B
    MorpheusCommand command;
    memset(&command, 0, sizeof(command));
    
    command.type = (is_pairing_mode) ? MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE :
        MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE;
    command.version = PROTOBUF_VERSION;
    if(morpheus_ble_reply_protobuf(&command))
    {
        _led_pairing_mode();
    }

}

void _start_dfu_process(void)
{
    // TODO: Begin DFU here.
}

void message_ble_on_protobuf_command(const MSG_Data_t* data_page, const MorpheusCommand* command)
{
    MSG_Base_AcquireDataAtomic(data_page);
    // A protobuf actually occupy multiple pages..

    switch(command->type)
    {
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:
            _morpheus_switch_mode(true);
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE:
            _morpheus_switch_mode(false);
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_WIFI_ENDPOINT:
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT:
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE:
            if(message_ble_route_data_to_cc3200(data_page) == FAIL)
            {
                PRINTS("Pass data to CC3200 failed, not enough memory.\r\n");
            }
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_EREASE_PAIRED_PHONE:
            _erase_bonded_users();
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_PILL:
        {
            MSG_Data_t* account_id_page = command->accountId.arg;
            if(account_id_page){
                
                message_ble_pill_pairing_begin(account_id_page);
                PRINTS("Account id: ");
                PRINTS(account_id_page->buf);
                PRINTS("\r\n");
                
            }
        }
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_UNPAIR_PILL:
        {
            MSG_Data_t* pill_id_page = command->deviceId.arg;
            if(pill_id_page)
            {
                message_ble_remove_pill(pill_id_page);
            }
        }
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_DFU_BEGIN:
            _start_dfu_process();
            break;
        default:
            break;
    }

    MSG_Base_ReleaseDataAtomic(data_page);

}

