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
    app_timer_id_t boot_timer;
} self;



static MSG_Status _destroy(void){
    _release_pending_resources();
    return SUCCESS;
}

static MSG_Status _flush(void){
    return SUCCESS;
}


static void _register_pill(){
    if(self.pill_pairing_request.account_id && self.pill_pairing_request.device_id){
        // now we have both the user's account id and pill id
        // compose the pairing request protobuf and send the shit to CC3200.

        MorpheusCommand pairing_command;
        memset(&pairing_command, 0, sizeof(pairing_command));

        pairing_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_PILL;
        pairing_command.version = PROTOBUF_VERSION;
        pairing_command.deviceId.arg = self.pill_pairing_request.device_id;
        pairing_command.accountId.arg = self.pill_pairing_request.account_id;

        size_t protobuf_len = 0;
        if(!morpheus_ble_encode_protobuf(&pairing_command, NULL, &protobuf_len))
        {
            morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
        }else{
            MSG_Data_t* compact_page = MSG_Base_AllocateDataAtomic(protobuf_len);
            if(morpheus_ble_encode_protobuf(&pairing_command, compact_page->buf, &protobuf_len))
            {
                // Route data to CC3200, CC3200 is expected to do the registration on server.
                // Once the registration is done, CC3200 should send the same command back.
                message_ble_route_data_to_cc3200(compact_page);

            }else{
                morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
            }
            MSG_Base_ReleaseDataAtomic(compact_page);
        }

    }else{
        PRINTS("BT: Invalid pairing state\r\n");
    }

    _release_pending_resources();
}

static void _init_ble_stack(const MorpheusCommand* command)
{
    if(hble_get_device_id() != 0)
    {
        PRINTS("BLE already initialized!\r\n");
    }else{

        // TODO: Set the morpheus device id into BLE advertising data.
        // Jimmy need this
        if(command->deviceId.arg)
        {
            MSG_Data_t* data_page = (MSG_Data_t*)command->deviceId.arg;
            PRINTS("Hex device id:");
            PRINTS(data_page->buf);
            PRINTS("\r\n");

            nrf_delay_ms(100);

            uint64_t device_id = 0;
            
            if(!hble_hex_to_uint64_device_id(data_page->buf, &device_id))
            {
                PRINTS("Get device id failed.\r\n");
                APP_ASSERT(0);
            }
            

            hble_stack_init();

#ifdef BONDING_REQUIRED   
            hble_bond_manager_init();
#endif
            // append something to device name
            char device_name[strlen(BLE_DEVICE_NAME)+4];
            memcpy(device_name, BLE_DEVICE_NAME, strlen(BLE_DEVICE_NAME));
            uint8_t id = *(uint8_t *)NRF_FICR->DEVICEID;
            device_name[strlen(BLE_DEVICE_NAME)] = '-';
            device_name[strlen(BLE_DEVICE_NAME)+1] = hex[(id >> 4) & 0xF];
            device_name[strlen(BLE_DEVICE_NAME)+2] = hex[(id & 0xF)];
            device_name[strlen(BLE_DEVICE_NAME)+3] = '\0';
            hble_params_init(device_name, device_id);
            hble_services_init();

            ble_uuid_t service_uuid = {
                .type = hello_type,
                .uuid = BLE_UUID_MORPHEUS_SVC
            };

            hble_advertising_init(service_uuid);
            
            hble_advertising_start();
        }else{
            PRINTS("INIT Error, no device id presented.");
        }
    }
}

static MSG_Status _on_data_arrival(MSG_Address_t src, MSG_Address_t dst,  MSG_Data_t* data){
    if(!data){
        PRINTS("Data is NULL\r\n");
        APP_ASSERT(0);
    }

    PRINTS("Enter _on_data_arrival\r\n");
    if(SUCCESS != MSG_Base_AcquireDataAtomic(data))
    {
        PRINTS("WTF!\r\n");
        return FAIL;
    }

    MSG_BLECommand_t * cmd = (MSG_BLECommand_t *)data->buf;

    // As a user of the stack, we should not assume anything about the source and destination
    // target, always check, or we will have bug like this.
    if(dst.submodule == 0 && dst.module == BLE && src.module == ANT){
        PRINTS("ANT to BLE Command received\r\n");

        switch(cmd->cmd){
            default:
            case BLE_PING:
                break;
                /*
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
                    
                    size_t hex_string_len = 0;
                    hble_uint64_to_hex_device_id(cmd->param.pill_uid, NULL, &hex_string_len);
                    char hex_string[hex_string_len];
                    hble_uint64_to_hex_device_id(cmd->param.pill_uid, hex_string, &hex_string_len);
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
                */
        }

    }
    

    if(src.module == SSPI && dst.submodule == 0 && dst.module == BLE){
        PRINTS("SPI to BLE Command received\r\n");

        MorpheusCommand command;
        if(morpheus_ble_decode_protobuf(&command, data->buf, data->len))
        {
            switch(command.type)
            {
                case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
                {
                    PRINTS("BLE init command received\r\n");
                    _init_ble_stack(&command);
                    morpheus_ble_free_protobuf(&command);
                    

                }
                break;
                case MorpheusCommand_CommandType_MORPHEUS_COMMAND_MORPHEUS_DFU_BEGIN:
                    _start_morpheus_dfu_process();  // It's just that simple.
                break;
                default:
                {
                // protobuf, dump the thing straight back?
                PRINTS(">>>>>>>>>>>Protobuf to PHONE\r\n");

                morpheus_ble_free_protobuf(&command);  // Always free protobuf here.
                hlo_ble_notify(0xB00B, data->buf, data->len,
                    &(struct hlo_ble_operation_callbacks){morpheus_ble_on_notify_completed, morpheus_ble_on_notify_failed, data});

                return SUCCESS;  // THIS IS A RETURN! DONOT release data here, it will be released in the callback.
                }
            }
        }else{
            // if(morpheus_ble_decode_protobuf(&command, data->buf, data->len))
            // decode failed.
            PRINTS("Protobuf decode failed\r\n");
            morpheus_ble_free_protobuf(&command);
            
        }
        
        
    }

    MSG_Base_ReleaseDataAtomic(data);
    return SUCCESS;
    
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

#ifndef HAS_CC3200
        // Echo test
        hlo_ble_notify(0xB00B, data->buf, data->len, 
                    &(struct hlo_ble_operation_callbacks){
                        morpheus_ble_on_notify_completed, 
                        morpheus_ble_on_notify_failed, 
                        data
                    });
        // DO NOT release data because callback will do it.
#else
        MSG_Base_ReleaseDataAtomic(data);
#endif
        
        
        return SUCCESS;
    }else{
        PRINTS("Acquire data failed, cannot route data to cc3200\r\n");
        return FAIL;
    }
}

static void _request_device_id(void* context)
{
    if(hble_get_device_id() != 0){
        PRINTS("Boot completed!\r\n");
        return;
    }else{
        PRINTS("No device_id detected, retry...\r\n");
    }

    // Tests
    // self.pill_pairing_request.device_id = MSG_Base_AllocateStringAtomic("test pill id");
    MorpheusCommand get_device_id_command;
    memset(&get_device_id_command, 0, sizeof(get_device_id_command));
    get_device_id_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID;
    get_device_id_command.version = PROTOBUF_VERSION;

#ifdef HAS_CC3200

    
    size_t protobuf_len = 0;
    if(!morpheus_ble_encode_protobuf(&get_device_id_command, NULL, &protobuf_len))
    {
        PRINTS("Failed to encode protobuf. Retry to boot...\r\n");
    }else{

        MSG_Data_t* data_page = MSG_Base_AllocateDataAtomic(protobuf_len);
        if(!data_page)
        {
            PRINTS("No memory. Retry to boot...\r\n");
            
        }else{

            memset(data_page->buf, 0, data_page->len);
            if(morpheus_ble_encode_protobuf(&get_device_id_command, data_page->buf, &protobuf_len))
            {
                self.parent->dispatch((MSG_Address_t){BLE, 0},(MSG_Address_t){SSPI, 1}, data_page);
            }
            MSG_Base_ReleaseDataAtomic(data_page);
        }
    }

    app_timer_start(self.boot_timer, BLE_BOOT_RETRY_INTERVAL, NULL);
#endif
}

static MSG_Status _init(){
    self.pill_pairing_request = (struct pill_pairing_request){0};
    app_timer_create(&self.timer_id, APP_TIMER_MODE_SINGLE_SHOT, _pill_pairing_time_out);
    

    

#ifdef HAS_CC3200

    app_timer_create(&self.boot_timer, APP_TIMER_MODE_SINGLE_SHOT, _request_device_id);
    _request_device_id(NULL);
    
#else
    
    // Tests
    // self.pill_pairing_request.device_id = MSG_Base_AllocateStringAtomic("test pill id");
    MorpheusCommand get_device_id_command;
    memset(&get_device_id_command, 0, sizeof(get_device_id_command));
    get_device_id_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID;
    get_device_id_command.version = PROTOBUF_VERSION;

    char* fake_device_id = "0123456789AB";
    MSG_Data_t* device_id_page = MSG_Base_AllocateStringAtomic(fake_device_id);

    if(!device_id_page)
    {
        PRINTS("No memory.\r\n");
        return FAIL;
    }

    get_device_id_command.deviceId.arg = device_id_page;

    size_t protobuf_len = 0;
    if(!morpheus_ble_encode_protobuf(&get_device_id_command, NULL, &protobuf_len))
    {
        MSG_Base_ReleaseDataAtomic(device_id_page);
        return FAIL;
    }

    MSG_Data_t* data_page = MSG_Base_AllocateDataAtomic(protobuf_len);
    if(!data_page)
    {
        PRINTS("No memory.\r\n");
        MSG_Base_ReleaseDataAtomic(device_id_page);
        return FAIL;
    }

    memset(data_page->buf, 0, data_page->len);
    if(!morpheus_ble_encode_protobuf(&get_device_id_command, data_page->buf, &protobuf_len))
    {   
        MSG_Base_ReleaseDataAtomic(device_id_page);
        MSG_Base_ReleaseDataAtomic(data_page);
        return FAIL;
    }
    
    self.parent->dispatch((MSG_Address_t){SSPI, 1},(MSG_Address_t){BLE, 0}, data_page);
    
    MSG_Base_ReleaseDataAtomic(device_id_page);
    MSG_Base_ReleaseDataAtomic(data_page);
    
    PRINTS("Debug SPI init command sent\r\n");

    

#endif

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
#ifdef HAS_CC3200  // If don't have CC, the route_data_to_cc function will reply
    morpheus_ble_reply_protobuf(&command);
#endif    

}

static void _start_morpheus_dfu_process(void)
{
    // TODO: Begin DFU here.
    REBOOT_TO_DFU();
}

static void _start_pill_dfu_process(uint64_t pill_id)
{
    // TODO: Begin Pill DFU here.
    ant_pill_dfu_begin(pill_id);
}

static void _pair_morpheus(MorpheusCommand* command)
{
    if(!command->accountId.arg)
    {
        PRINTS("No account id for pairing!\r\n");
        return;
    }else{
        MSG_Base_AcquireDataAtomic(command->accountId.arg);
    }

    MSG_Data_t* device_id_page = MSG_Base_AllocateDataAtomic(17);
    if(!device_id_page)
    {
        PRINTS("Not enough memory.\r\n");
        morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
    }else{
        memset(device_id_page->buf, 0, device_id_page->len);
        size_t device_id_len = device_id_page->len;

        if(hble_uint64_to_hex_device_id(hble_get_device_id(), device_id_page->buf, &device_id_len))
        {
            MorpheusCommand pair_command;
            memset(&pair_command, 0, sizeof(MorpheusCommand));

            pair_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE;
            pair_command.version = PROTOBUF_VERSION;
            pair_command.accountId.arg = command->accountId.arg;
            PRINTS("account id: ");
            PRINTS(((MSG_Data_t*)pair_command.accountId.arg)->buf);
            PRINTS("\r\n");

            pair_command.deviceId.arg = device_id_page;

            PRINTS("device id: ");
            PRINTS(((MSG_Data_t*)pair_command.deviceId.arg)->buf);
            PRINTS("\r\n");

            size_t proto_len = 0;
            if(!morpheus_ble_encode_protobuf(&pair_command, NULL, &proto_len))
            {
                morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
            }else{
                MSG_Data_t* proto_page = MSG_Base_AllocateDataAtomic(proto_len);
                if(!proto_len){
                    PRINTS("Not enough memory.\r\n");
                    morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
                }else{
                    morpheus_ble_encode_protobuf(&pair_command, proto_page->buf, &proto_len);  // I assume it will make it if we reach this point
                    if(message_ble_route_data_to_cc3200(proto_page) == FAIL)
                    {
                        PRINTS("Pass data to CC3200 failed, not enough memory.\r\n");
                        morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
                    }
                    MSG_Base_ReleaseDataAtomic(proto_page);
                }

            }

        }else{
            PRINTS("Convert device id failed\r\n");
            morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
        }

        MSG_Base_ReleaseDataAtomic(device_id_page);
    }

    MSG_Base_ReleaseDataAtomic(command->accountId.arg);
}

void message_ble_on_protobuf_command(MSG_Data_t* data_page, const MorpheusCommand* command)
{
    MSG_Base_AcquireDataAtomic(data_page);
    // A protobuf actually occupy multiple pages..
    PRINTS("data_page addr: ");
    PRINT_HEX(&data_page, sizeof(data_page));
    PRINTS("\r\n");

    switch(command->type)
    {
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:
            _morpheus_switch_mode(true);
            if(message_ble_route_data_to_cc3200(data_page) == FAIL)
            {
                PRINTS("Pass data to CC3200 failed, not enough memory.\r\n");
                morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
            }
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE:
            _morpheus_switch_mode(false);
            if(message_ble_route_data_to_cc3200(data_page) == FAIL)
            {
                PRINTS("Pass data to CC3200 failed, not enough memory.\r\n");
                morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
            }
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_WIFI_ENDPOINT:
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT:
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_PILL:
            if(message_ble_route_data_to_cc3200(data_page) == FAIL)
            {
                PRINTS("Pass data to CC3200 failed, not enough memory.\r\n");
                morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
            }
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE:
            _pair_morpheus(command);
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_EREASE_PAIRED_PHONE:
            _erase_bonded_users();
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
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_MORPHEUS_DFU_BEGIN:
            _start_morpheus_dfu_process();
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DFU_BEGIN:
            {
                MSG_Data_t* device_id_page = command->deviceId.arg;
                if(!device_id_page){
                    PRINTS("No device id found for DFU.\r\n");
                    return;
                }
                uint64_t pill_id = 0;
                hble_hex_to_uint64_device_id(device_id_page->buf, &pill_id);
                _start_pill_dfu_process(pill_id);
            }
            break;
        default:
            break;
    }

    MSG_Base_ReleaseDataAtomic(data_page);

}

