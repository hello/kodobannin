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
#include "ble_bondmngr.h"
#include "nrf_delay.h"
#include "hlo_queue.h"

#ifdef ANT_STACK_SUPPORT_REQD
#include "message_ant.h"
#include "ant_user.h"
#endif

void pwr_reset_3200();

static void _release_pending_resources();
static void _on_notify_failed(void* data_page);
static void _on_notify_completed(const void* data, void* data_page);
static void _dequeue_tx(void);
static bool _queue_tx(MSG_Data_t * msg);

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    struct pill_pairing_request pill_pairing_request;
    app_timer_id_t timer_id;
    app_timer_id_t boot_timer;
    boot_status boot_state;
    int ready_to_send;
    struct hlo_queue_t * tx_queue;
    uint16_t cached_bond_count;
    bool cached_is_pairing;
} self;



static MSG_Status _destroy(void){
    _release_pending_resources();
    return SUCCESS;
}

static MSG_Status _flush(void){
    return SUCCESS;
}

static void _start_morpheus_dfu_process(void)
{
    // TODO: Begin DFU here.
    REBOOT_TO_DFU();
}

static void _sync_pairing_info(bool is_pairing_mode, uint16_t bond_count){
    MorpheusCommand advertising_command;
    memset(&advertising_command, 0, sizeof(advertising_command));
    advertising_command.type = is_pairing_mode ? MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:
        MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE;
    advertising_command.ble_bond_count = bond_count;
    advertising_command.has_ble_bond_count = true;
    
    if(!morpheus_ble_route_protobuf_to_cc3200(&advertising_command))
    {
        PRINTS("Route protobuf to middle board failed\r\n");
    }

    self.cached_is_pairing = is_pairing_mode;
    self.cached_bond_count = bond_count;
}

static void _on_advertise_started(bool is_pairing_mode, uint16_t bond_count)
{
    PRINTS("Advertisement Started: ");
    PRINT_HEX(&is_pairing_mode, 1);
    PRINT_HEX(&bond_count, 1);
    PRINTS("\r\n");
    _sync_pairing_info(is_pairing_mode, bond_count);
}

static void _on_bond_finished(ble_bondmngr_evt_type_t bond_type)
{
    MorpheusCommand ble_command;
    memset(&ble_command, 0, sizeof(ble_command));
    ble_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PHONE_BLE_BONDED;
    if(!morpheus_ble_route_protobuf_to_cc3200(&ble_command))
    {
        PRINTS("Route protobuf to middle board failed\r\n");
    }
}

static void _on_connected(void)
{
    MorpheusCommand ble_command;
    memset(&ble_command, 0, sizeof(ble_command));
    ble_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PHONE_BLE_CONNECTED;
    if(!morpheus_ble_route_protobuf_to_cc3200(&ble_command))
    {
        PRINTS("Route protobuf to middle board failed\r\n");
    }
}


static void _register_pill(){
    if(self.pill_pairing_request.account_id && self.pill_pairing_request.device_id){
        // now we have both the user's account id and pill id
        // compose the pairing request protobuf and send the shit to CC3200.

        MorpheusCommand pairing_command;
        memset(&pairing_command, 0, sizeof(pairing_command));

        pairing_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_PILL;
        pairing_command.deviceId.arg = self.pill_pairing_request.device_id;
        pairing_command.accountId.arg = self.pill_pairing_request.account_id;

        if(!morpheus_ble_route_protobuf_to_cc3200(&pairing_command))
        {
            morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_DATA_ERROR);
        }

    }else{
        PRINTS("BT: Invalid pairing state\r\n");
    }

    _release_pending_resources();
}


static void _sync_device_id()
{
    size_t hex_device_id_len = 0;
    if(!hble_uint64_to_hex_device_id(GET_UUID_64(), NULL, &hex_device_id_len))
    {
        PRINTS("Get device id len failed.\r\n");
        nrf_delay_ms(100);
        APP_ASSERT(0);
    }

    MSG_Data_t* device_id_page = MSG_Base_AllocateDataAtomic(hex_device_id_len + 1);
    if(!device_id_page){
        PRINTS("Get device id page failed.\r\n");
        nrf_delay_ms(100);
        APP_ASSERT(0);
    }

    memset(device_id_page->buf, 0, hex_device_id_len + 1);
    if(!hble_uint64_to_hex_device_id(GET_UUID_64(), device_id_page->buf, &hex_device_id_len))
    {
        PRINTS("Get device id failed.\r\n");
        MSG_Base_ReleaseDataAtomic(device_id_page);
        nrf_delay_ms(100);
        APP_ASSERT(0);
    }

    MorpheusCommand sync_device_id_command;
    memset(&sync_device_id_command, 0, sizeof(sync_device_id_command));
    sync_device_id_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID;
    sync_device_id_command.deviceId.arg = device_id_page;

    //write key
    sync_device_id_command.has_aes_key = true;
    sync_device_id_command.aes_key.size = AES128_BLOCK_SIZE;
    memcpy(sync_device_id_command.aes_key.bytes, (uint8_t*)NRF_FICR->ER, AES128_BLOCK_SIZE);

    //write version
    sync_device_id_command.has_top_version = true;
    memset(sync_device_id_command.top_version, 0, sizeof(sync_device_id_command.top_version));
    memcpy(sync_device_id_command.top_version, FW_VERSION_STRING, sizeof(FW_VERSION_STRING));

    //write bond count
    uint16_t bond_count = BLE_BONDMNGR_MAX_BONDED_CENTRALS;
    APP_OK(ble_bondmngr_central_ids_get(NULL, &bond_count));
    sync_device_id_command.has_ble_bond_count = true;
    sync_device_id_command.ble_bond_count = bond_count;

    if(!morpheus_ble_route_protobuf_to_cc3200(&sync_device_id_command))
    {
        PRINTS("Encode sync deviceId protobuf failed.\r\n");
        nrf_delay_ms(100);
        APP_ASSERT(0);
    }

    morpheus_ble_free_protobuf(&sync_device_id_command);
}


static void _init_ble_stack(const MorpheusCommand* command)
{
    // append something to device name
    char device_name[strlen(BLE_DEVICE_NAME)+4];
    memcpy(device_name, BLE_DEVICE_NAME, strlen(BLE_DEVICE_NAME));
    uint8_t id = *(uint8_t *)NRF_FICR->DEVICEID;
    device_name[strlen(BLE_DEVICE_NAME)] = '-';
    device_name[strlen(BLE_DEVICE_NAME)+1] = hex[(id >> 4) & 0xF];
    device_name[strlen(BLE_DEVICE_NAME)+2] = hex[(id & 0xF)];
    device_name[strlen(BLE_DEVICE_NAME)+3] = '\0';

    switch(self.boot_state)
    {
        case BOOT_COMPLETED:
        PRINTS("BLE already initialized!\r\n");
        break;
        case BOOT_CHECK:
        {
            if(command->deviceId.arg){
                //backward compatibility, instead of supporting EVT middle board fw ad infinitum we just warn user instead
                nrf_delay_ms(100);
                PRINTS("WARNING: Deprecated GET_DEVICE_ID behavior\r\n");
                nrf_delay_ms(100);
            }
            self.boot_state = BOOT_SYNC_DEVICE_ID;  // set state, wait for the boot timer call _sync_device_id()
            _sync_device_id();
        }
        break;
        case BOOT_SYNC_DEVICE_ID:  // The sync device id command was sent
        {
            PRINTS("SYNC DEVICEID\r\n");
            
            hble_params_init(device_name, GET_UUID_64(), command->firmware_build);
            hble_services_init();

            ble_uuid_t service_uuid = {
                .type = hello_type,
                .uuid = BLE_UUID_MORPHEUS_SVC
            };

            hble_advertising_init(service_uuid);
            self.boot_state = BOOT_COMPLETED;  // set state, wait for boot timer start advertising and end itself.
        }
        break;

    }
}

static bool _is_bond_db_full()
{
    uint16_t paired_users_count = BLE_BONDMNGR_MAX_BONDED_CENTRALS;
    APP_OK(ble_bondmngr_central_ids_get(NULL, &paired_users_count));

    if(paired_users_count == BLE_BONDMNGR_MAX_BONDED_CENTRALS)
    {
        PRINTS("Pairing database full.\r\n");
        return true;
    }

    return false;
}

static void _hold_to_enter_pairing_mode()
{
    if(_is_bond_db_full()){
        hble_refresh_bonds(ERASE_1ST_BOND, true);
    }else{
        hble_refresh_bonds(BOND_NO_OP, true);
    }
}

static void _hold_to_enter_normal_mode()
{
    hble_refresh_bonds(BOND_NO_OP, false);
}

static MSG_Status _route_protobuf_to_ble(MSG_Data_t * data){
    MorpheusCommand command;
    if(morpheus_ble_decode_protobuf(&command, data->buf, data->len)){
        switch(command.type){
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
                if(self.boot_state == BOOT_COMPLETED){
                    //allows resyncing of topboard information
                    _sync_device_id();
                    _sync_pairing_info(self.cached_is_pairing, self.cached_bond_count);
                }
                // else, fallback to normal boot sequence, the same code in SYNC_DEVICE_ID
                // DONOT put a break here, it is intended to fall to 
                // MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID:
                PRINTS("BLE init command received: ");
                PRINT_HEX(&command.type, sizeof(command.type));
                PRINTS("\r\n");
                if(self.boot_state != BOOT_COMPLETED){
                    _init_ble_stack(&command);
                }
                break;
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_MORPHEUS_DFU_BEGIN:
                PRINTS("DFU from CC3200..\r\n");
                _start_morpheus_dfu_process();  // It's just that simple.
                break;
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:
                PRINTS("switch to pairing..\r\n");
                _queue_tx(data);
                _hold_to_enter_pairing_mode();
                break;
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE:
                PRINTS("switch to normal..\r\n");
                _queue_tx(data);
                _hold_to_enter_normal_mode();
                break;
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_ERASE_PAIRED_PHONE:
                PRINTS("erase phone..\r\n");
                _queue_tx(data);
                hble_refresh_bonds(ERASE_OTHER_BOND, true);
                break;
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET:
                PRINTS("Factory reset from CC3200..\r\n");
                _queue_tx(data);
                hble_refresh_bonds(ERASE_ALL_BOND, true);
                pwr_reset_3200();
                break;
            default:
                // protobuf, dump the thing straight back?
                PRINTS(">>>Proto to PHONE\r\n");
                _queue_tx(data);
                break;
        }
        morpheus_ble_free_protobuf(&command);
    }else{
        PRINTS("Protobuf decode failed\r\n");
        morpheus_ble_free_protobuf(&command);
        return FAIL;
    }
    return SUCCESS;
}

static MSG_Status _on_data_arrival(MSG_Address_t src, MSG_Address_t dst,  MSG_Data_t* data){
    PRINTS("Enter _on_data_arrival\r\n");
    switch(dst.submodule){
        default:
        case MSG_BLE_PING:
            break;
        case MSG_BLE_ACK_DEVICE_REMOVED:
            break;
        case MSG_BLE_ACK_DEVICE_ADDED:
            if(data){
                uint64_t pill_uid = *((uint64_t*)data->buf);
                size_t hex_string_len = 0;
                char hex_string[17];

                app_timer_stop(self.timer_id);
                ANT_UserSetPairing(0);
                hble_uint64_to_hex_device_id(pill_uid, NULL, &hex_string_len);

                if(sizeof(hex_string) >= hex_string_len){
                    hble_uint64_to_hex_device_id(pill_uid, hex_string, &hex_string_len);
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

            }
            break;
        case MSG_BLE_BOOT_RADIO:
            {
                MorpheusCommand command = {0};
                PRINTS("Booting Radio\r\n");
                command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID;
                _init_ble_stack(&command);
                command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID;
                _init_ble_stack(&command);
            }
            break;
        case MSG_BLE_DEFAULT_CONNECTION:
            if(data){
                return _route_protobuf_to_ble(data);
            }else{
                PRINTS("No data received from:");
                PRINT_HEX(&src.module, 1);
                PRINTS("\r\n");
            }
    }
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

static void _pill_pairing_time_out(void* context)
{
    _release_pending_resources();
    morpheus_ble_reply_protobuf_error(ErrorType_TIME_OUT);
    ANT_UserSetPairing(0);
    PRINTS("Pill pairing time out.\r\n");
}



MSG_Status message_ble_pill_pairing_begin(MSG_Data_t* account_id_page)
{
    _release_pending_resources();

    MSG_Base_AcquireDataAtomic(account_id_page);
    self.pill_pairing_request.account_id = account_id_page;

    // Send notification to ANT? Actually at this time ANT can just send back device id without being notified.
#ifdef HAS_CC3200
    PRINTS("Waiting the pill to reply...\r\n");
    if( self.timer_id )
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

MSG_Status message_ble_route_data_to_cc3200(MSG_Data_t* data){
    
    if(SUCCESS == MSG_Base_AcquireDataAtomic(data)){
        self.parent->dispatch((MSG_Address_t){BLE, 1},(MSG_Address_t){SSPI, 1}, data);

#ifndef HAS_CC3200
        // Echo test
        _queue_tx(data);
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

static void _cc3200_boot_check(){
    self.boot_state = BOOT_CHECK;
    // Tests
    // self.pill_pairing_request.device_id = MSG_Base_AllocateStringAtomic("test pill id");
    MorpheusCommand get_device_id_command;
    memset(&get_device_id_command, 0, sizeof(get_device_id_command));
    get_device_id_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID;
    uint16_t bond_count = BLE_BONDMNGR_MAX_BONDED_CENTRALS;
    APP_OK(ble_bondmngr_central_ids_get(NULL, &bond_count));

    get_device_id_command.has_ble_bond_count = true;
    get_device_id_command.ble_bond_count = bond_count;

#ifdef HAS_CC3200
    morpheus_ble_route_protobuf_to_cc3200(&get_device_id_command);
#endif
}

static void _on_boot_timer(void* context)
{
    switch(self.boot_state)
    {
        case BOOT_COMPLETED:
        hble_advertising_start();
        nrf_delay_ms(100);
        PRINTS("Boot completed!\r\n");
        break;

        case BOOT_CHECK:
        PRINTS("No device_id detected, retry...\r\n");
        _cc3200_boot_check();
        break;

        case BOOT_SYNC_DEVICE_ID:
        PRINTS("CC3200 seems to stuck, retry...\r\n");
        _sync_device_id();
        break;

        default:
        break;
    }

#ifdef HAS_CC3200
    if(self.boot_state != BOOT_COMPLETED){
        app_timer_start(self.boot_timer, BLE_BOOT_RETRY_INTERVAL, NULL);
    }
#endif
}
static void _on_notify_completed(const void* data, void* data_page){
    MSG_Base_ReleaseDataAtomic((MSG_Data_t*)data_page);
    uint32_t pool_free = MSG_Base_FreeCount();
    PRINTS("heap free: ");
    PRINT_HEX(&pool_free, sizeof(pool_free));
    PRINTS("\r\n");
    _dequeue_tx();
}

static void _on_notify_failed(void* data_page){
    MSG_Base_ReleaseDataAtomic((MSG_Data_t*)data_page);
    uint32_t pool_free = MSG_Base_FreeCount();
    PRINTS("heap free: ");
    PRINT_HEX(&pool_free, sizeof(pool_free));
    PRINTS("\r\n");
    _dequeue_tx();
}
static void _dequeue_tx(void){
    MSG_Data_t * next = NULL;
    if(hlo_queue_filled_size(self.tx_queue) >= sizeof(&next)){
        PRINTS("tx_ble:");

        CRITICAL_REGION_ENTER();
        hlo_queue_read(self.tx_queue, (unsigned char*)&next, sizeof(&next));
        self.ready_to_send = 0;
        CRITICAL_REGION_EXIT();

        PRINT_HEX(&next->len, 2);
        PRINTS("\r\n");
        if(next){
            hlo_ble_notify(0xB00B, next->buf, next->len,
                    &(struct hlo_ble_operation_callbacks){_on_notify_completed, _on_notify_failed, next});
        }
    }else{
        PRINTS("no more data\r\n");

        CRITICAL_REGION_ENTER();
        self.ready_to_send = 1;
        CRITICAL_REGION_EXIT();
    }
}
static bool _queue_tx(MSG_Data_t * msg){
    if(msg && hlo_queue_empty_size(self.tx_queue) >= sizeof(&msg)){
        MSG_Base_AcquireDataAtomic(msg);

        CRITICAL_REGION_ENTER();
        hlo_queue_write(self.tx_queue, (unsigned char*)&msg, sizeof(&msg));
        if(self.ready_to_send){
            _dequeue_tx();
        }
        CRITICAL_REGION_EXIT();

        return true;
    }
    return false;
}

static MSG_Status _init(){

    hble_stack_init();

    self.tx_queue = hlo_queue_init(32);
    APP_ASSERT(self.tx_queue);
    self.ready_to_send = 1;

#ifdef BONDING_REQUIRED     
    hble_bond_manager_init();
    nrf_delay_ms(200);    
#endif

    self.pill_pairing_request = (struct pill_pairing_request){0};
    app_timer_create(&self.timer_id, APP_TIMER_MODE_SINGLE_SHOT, _pill_pairing_time_out);
    
#ifdef HAS_CC3200
    hble_set_advertise_callback(_on_advertise_started);
    hble_set_connected_callback(_on_connected);
    hble_set_bond_status_callback(_on_bond_finished);
    app_timer_create(&self.boot_timer, APP_TIMER_MODE_SINGLE_SHOT, _on_boot_timer);
    _cc3200_boot_check();
    app_timer_start(self.boot_timer, BLE_BOOT_RETRY_INTERVAL, NULL);
    
#else
    //use boot command instead
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

    MorpheusCommand command;
    memset(&command, 0, sizeof(command));
    
    command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_ERASE_PAIRED_PHONE;
    morpheus_ble_reply_protobuf(&command);
}


static void _morpheus_switch_mode(bool is_pairing_mode)
{
    // reply to 0xB00B
    MorpheusCommand command;
    memset(&command, 0, sizeof(command));

    command.type = (is_pairing_mode) ? MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE :
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE;
    morpheus_ble_reply_protobuf(&command);
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

    MSG_Data_t* device_id_page = MSG_Base_AllocateDataAtomic(DEVICE_ID_SIZE * 2 + 1);  // Fark this is a mac address
    if(!device_id_page)
    {
        PRINTS("Fail to alloc device id\r\n");
        morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
    }else{
        memset(device_id_page->buf, 0, device_id_page->len);
        size_t device_id_len = device_id_page->len;

        if(hble_uint64_to_hex_device_id(hble_get_device_id(), device_id_page->buf, &device_id_len))
        {
            MorpheusCommand pair_command;
            memset(&pair_command, 0, sizeof(MorpheusCommand));

            pair_command.type = MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE;
            pair_command.accountId.arg = command->accountId.arg;
            PRINTS("account id: ");
            PRINTS(((MSG_Data_t*)pair_command.accountId.arg)->buf);
            PRINTS("\r\n");

            pair_command.deviceId.arg = device_id_page;

            PRINTS("device id: ");
            PRINTS(((MSG_Data_t*)pair_command.deviceId.arg)->buf);
            PRINTS("\r\n");
            if(!morpheus_ble_route_protobuf_to_cc3200(&pair_command))
            {
                PRINTS("Rount pairing data to cc3200 failed\r\n");
                morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
            }
            PRINTS("Rount pairing data to cc3200 done!\r\n");

        }else{
            PRINTS("Convert device id failed\r\n");
            morpheus_ble_reply_protobuf_error(ErrorType_INTERNAL_OPERATION_FAILED);
        }

        MSG_Base_ReleaseDataAtomic(device_id_page);
    }

    MSG_Base_ReleaseDataAtomic(command->accountId.arg);
}

bool morpheus_ble_route_protobuf_to_cc3200(MorpheusCommand* command)
{
    size_t proto_len = 0;
    if(!morpheus_ble_encode_protobuf(command, NULL, &proto_len))
    {
        PRINTS("encode failed\r\n");
        return false;
    }

    PRINTS("Proto len: ");
    PRINT_HEX(&proto_len, sizeof(proto_len));
    PRINTS("\r\n");


    MSG_Data_t* proto_page = MSG_Base_AllocateDataAtomic(proto_len);
    if(!proto_page){
        PRINTS(MSG_NO_MEMORY);
        return false;
    }


    morpheus_ble_encode_protobuf(command, proto_page->buf, &proto_len);  // I assume it will make it if we reach this point
    self.parent->dispatch((MSG_Address_t){BLE, 1},(MSG_Address_t){SSPI, 1}, proto_page);
    MSG_Base_ReleaseDataAtomic(proto_page);

    return true;
}

static void _pass_to_mid_and_handle_error(MSG_Data_t * data_page){
    if(message_ble_route_data_to_cc3200(data_page) == FAIL){
        PRINTS(MSG_NO_MEMORY);
        morpheus_ble_reply_protobuf_error(ErrorType_DEVICE_NO_MEMORY);
    }
}
//from phone
void message_ble_on_protobuf_command(MSG_Data_t* data_page, MorpheusCommand* command)
{
    MSG_Base_AcquireDataAtomic(data_page);
    // A protobuf actually occupy multiple pages..
    PRINTS("data_page addr: ");
    PRINT_HEX(&data_page, sizeof(data_page));
    PRINTS("\r\n");

    switch(command->type)
    {
        //pass the ball! (> ^ ^)>  ---- ()
        default:
            _pass_to_mid_and_handle_error(data_page);
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:
            _morpheus_switch_mode(true);
            _pass_to_mid_and_handle_error(data_page);
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE:
            _morpheus_switch_mode(false);
            _pass_to_mid_and_handle_error(data_page);
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_PILL:
            {
                MSG_Data_t* account_id_page = command->accountId.arg;
                message_ble_pill_pairing_begin(account_id_page);
            }
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE:
            _pair_morpheus(command);
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_ERASE_PAIRED_PHONE:
            _erase_bonded_users();
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_MORPHEUS_DFU_BEGIN:
            _start_morpheus_dfu_process();
            break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DFU_BEGIN:
            {
                MSG_Data_t* device_id_page = command->deviceId.arg;
                if(!device_id_page){
                    PRINTS("No device id found for DFU.\r\n");
                }else{
                    uint64_t pill_id = 0;
                    hble_hex_to_uint64_device_id(device_id_page->buf, &pill_id);
                    _start_pill_dfu_process(pill_id);
                }
            }
            break;
    }

    MSG_Base_ReleaseDataAtomic(data_page);

    uint32_t pool_free = MSG_Base_FreeCount();
    PRINTS("top heap: ");
    PRINT_HEX(&pool_free, sizeof(pool_free));
    PRINTS("\r\n");

}

