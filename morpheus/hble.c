// vi:noet:sw=4 ts=4

#include <stdlib.h>
#include <string.h>

#include <app_timer.h>
#include <ble.h>
#include <ble_advdata.h>
#include <ble_conn_params.h>
#include <ble_bas.h>
#include <ble_dis.h>
#include <ble_hci.h>
#include <nrf_sdm.h>
#include <nrf_soc.h>
#include <pstorage.h>
#include <softdevice_handler.h>
#include "app.h"
#include "platform.h"

#ifdef BONDING_REQUIRED
#include "ble_bondmngr_cfg.h"
#include "ble_bondmngr.h"
#endif

#include "util.h"

#include "hble.h"

#include "morpheus_gatt.h"
#include "morpheus_ble.h"

extern uint8_t hello_type;
static uint16_t _morpheus_service_handle;

static ble_gap_sec_params_t _sec_params;

static bool _pairing_mode = false;

static ble_uuid_t _service_uuid;
static int16_t  _last_bond_central_id; 

static void _on_disconnect(void * p_event_data, uint16_t event_size)
{
    // Reset transmission layer, clean out error states.
	morpheus_ble_transmission_layer_reset();
#ifdef BONDING_REQUIRED
    APP_OK(ble_bondmngr_bonded_centrals_store());
#endif
    nrf_delay_ms(100);
    hble_advertising_start();

    if(MSG_Base_HasMemoryLeak()){
        PRINTS("Possible memory leak detected!\r\n");
    }else{
        PRINTS("No memory leak.\r\n");
    }
}

static void _on_advertise_timeout(void * p_event_data, uint16_t event_size)
{
    nrf_delay_ms(100);  // Due to nRF51 release note
    hble_advertising_start();
}

static void _on_ble_evt(ble_evt_t* ble_evt)
{
    //static ble_gap_evt_auth_status_t _auth_status;

    switch(ble_evt->header.evt_id) {
    case BLE_GAP_EVT_CONNECTED:
        // Reset transmission layer, clean out error states.
        morpheus_ble_transmission_layer_reset();
        // When new connection comes in, always set it back to non-pairing mode.
        hble_set_advertising_mode(false);
        break;
    case BLE_GAP_EVT_DISCONNECTED:
        app_sched_event_put(NULL, 0, _on_disconnect);
        break;
    case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
        APP_OK(sd_ble_gap_sec_params_reply(hlo_ble_get_connection_handle(),
                                       BLE_GAP_SEC_STATUS_SUCCESS,
                                       &_sec_params));
        break;
    case BLE_GAP_EVT_TIMEOUT:
        if (ble_evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_ADVERTISEMENT) {
            app_sched_event_put(NULL, 0, _on_advertise_timeout);
        }
        break;
    default:
        // No implementation needed.
        break;
    }
    
}


static void _ble_evt_dispatch(ble_evt_t* p_ble_evt)
{

#ifdef BONDING_REQUIRED
    ble_bondmngr_on_ble_evt(p_ble_evt);
#endif

    ble_conn_params_on_ble_evt(p_ble_evt);
    hlo_ble_on_ble_evt(p_ble_evt);
    
    _on_ble_evt(p_ble_evt);
    
}


static void _on_sys_evt(uint32_t sys_evt)
{
    PRINTS("_on_sys_evt: ");
    PRINT_HEX(&sys_evt, sizeof(sys_evt));
    PRINTS("\r\n");

#ifdef BONDING_REQUIRED
    pstorage_sys_event_handler(sys_evt);
#endif

}

static void _on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    switch(p_evt->evt_type) {
    case BLE_CONN_PARAMS_EVT_FAILED:
        PRINTS("BLE_CONN_PARAMS_EVT_FAILED\r\n");
        APP_OK(sd_ble_gap_disconnect(hlo_ble_get_connection_handle(), BLE_HCI_CONN_INTERVAL_UNACCEPTABLE));
        break;
    default:
        break;
    }
}



/**@brief Function for handling a Bond Manager error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void _bond_manager_error_handler(uint32_t nrf_error)
{
    PRINTS("_bond_manager_error_handler, err: ");
    PRINT_HEX(&nrf_error, sizeof(nrf_error));
    PRINTS("\r\n");

    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for handling the Bond Manager events.
 *
 * @param[in]   p_evt   Data associated to the bond manager event.
 */
static void _bond_evt_handler(ble_bondmngr_evt_t * p_evt)
{
    _last_bond_central_id = p_evt->central_id;
    PRINTS("last bonded central: ");
    PRINT_HEX(&_last_bond_central_id, sizeof(_last_bond_central_id));
    PRINTS("\r\n");
}

void hble_erase_other_bonded_central()
{
    if(!_last_bond_central_id)
    {
        PRINTS("Current user is not bonded!");
        return;
    }

    PRINTS("Current central: ");
    PRINT_HEX(&_last_bond_central_id, sizeof(_last_bond_central_id));
    PRINTS("\r\n");
    
    

    uint16_t paired_users_count = BLE_BONDMNGR_MAX_BONDED_CENTRALS;
    APP_OK(ble_bondmngr_central_ids_get(NULL, &paired_users_count));
    if(paired_users_count == 0)
    {
        PRINTS("No paired centrals found.\r\n");
        return;
    }

    PRINTS("Paired central count: ");
    PRINT_HEX(&paired_users_count, sizeof(paired_users_count));
    PRINTS("\r\n");

    uint16_t bonded_central_list[BLE_BONDMNGR_MAX_BONDED_CENTRALS];
    memset(bonded_central_list, 0, sizeof(bonded_central_list));

    APP_OK(ble_bondmngr_central_ids_get(bonded_central_list, &paired_users_count));
    for(int i = 0; i < paired_users_count; i++)
    {
        if(bonded_central_list[i] != _last_bond_central_id)
        {
            APP_OK(ble_bondmngr_bonded_central_delete(bonded_central_list[i]));
            PRINTS("Paired central ");
            PRINT_HEX(&bonded_central_list[i], sizeof(bonded_central_list[i]));
            PRINTS(" deleted.\r\n");
        }
    }
}


void hble_set_advertising_mode(bool pairing_mode)
{
    _pairing_mode = pairing_mode;
}

/**@brief Function for the Bond Manager initialization.
 */
void hble_bond_manager_init()
{

    ble_bondmngr_init_t bond_init_data;
    bool bonds_delete = false;

    //PRINTS("pstorage_init() done.\r\n");

    memset(&bond_init_data, 0, sizeof(bond_init_data));
    
    // Initialize the Bond Manager.
    bond_init_data.evt_handler             = _bond_evt_handler;
    bond_init_data.error_handler           = _bond_manager_error_handler;
    bond_init_data.bonds_delete            = bonds_delete;

    APP_OK(ble_bondmngr_init(&bond_init_data));
    //PRINTS("bond manager init.\r\n");
}


static void _on_conn_params_error(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static void _advertising_data_init(uint8_t flags){
    ble_advdata_t advdata;
    ble_advdata_t scanrsp;
    ble_uuid_t adv_uuids[] = {_service_uuid};

    PRINTS("Service UUID:");
    PRINT_HEX(&_service_uuid.uuid, sizeof(_service_uuid.uuid));
    PRINTS("\r\n");

    // Build and set advertising data
    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = true;
    advdata.flags.size = sizeof(flags);
    advdata.flags.p_data = &flags;
    memset(&scanrsp, 0, sizeof(scanrsp));
    scanrsp.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    scanrsp.uuids_complete.p_uuids = adv_uuids;
    APP_OK(ble_advdata_set(&advdata, &scanrsp));
}


void hble_advertising_init(ble_uuid_t service_uuid)
{
    _service_uuid = service_uuid;
}

void hble_advertising_start()
{
    ble_gap_adv_params_t adv_params = {};
    adv_params.type = BLE_GAP_ADV_TYPE_ADV_IND;
    adv_params.p_peer_addr = NULL;
    adv_params.fp = BLE_GAP_ADV_FP_ANY;
    adv_params.interval = APP_ADV_INTERVAL;
    adv_params.timeout = APP_ADV_TIMEOUT_IN_SECONDS;
    uint8_t flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;


#ifdef BONDING_REQUIRED
    // pairing mode means no whitelist.
    if(!_pairing_mode)                
    {
        uint8_t adv_mode = BLE_GAP_ADV_TYPE_ADV_IND; 
        adv_params.type = adv_mode;
        ble_gap_whitelist_t whitelist;
        //APP_OK(ble_bondmngr_whitelist_get(&whitelist));
        uint32_t err_code = ble_bondmngr_whitelist_get(&whitelist);
        
        if(err_code == NRF_SUCCESS)
        {
            if (whitelist.addr_count != 0 || whitelist.irk_count != 0)
            {
                adv_params.p_whitelist = &whitelist;
                adv_params.fp          = BLE_GAP_ADV_FP_FILTER_BOTH;
                flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

                PRINTS("whitelist retrieved. Advertising in normal mode.\r\n");
            }else{
                flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;  // Just make it clear what we want to do.
                PRINTS("NO whitelist retrieved. Advertising in pairing mode.\r\n");
            }
        }else{
            PRINTS("get whitelist error: ");
            PRINT_HEX(&err_code, sizeof(err_code));
            PRINTS("\r\n");
            flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
        }

        APP_OK(err_code);
        
    }
    else
    {
        flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;  // Just to make things clear
        PRINTS("Advertising in pairing mode.\r\n");
    }
#endif

    _advertising_data_init(flags);
    APP_OK(sd_ble_gap_adv_start(&adv_params));

    PRINTS("Advertising started.\r\n");
}

void hble_stack_init()
{

    // Register with the SoftDevice handler module for BLE events.
    APP_OK(softdevice_ble_evt_handler_set(_ble_evt_dispatch));

    // Register with the SoftDevice handler module for BLE events.
    APP_OK(softdevice_sys_evt_handler_set(_on_sys_evt));

}

bool hble_uint64_to_hex_device_id(uint64_t device_id, char* hex_device_id, size_t* len)
{
    if(!len)
    {
        return false;
    }

    if(!hex_device_id || sizeof(device_id) * 2 + 1 < (*len))
    {
        *len = sizeof(device_id) * 2 + 1;
        return false;
    }

    memset(hex_device_id, 0, *len);
    const char* hex_table = "0123456789ABCDEF";
    
    size_t index = 0;
    for(size_t i = 0; i < sizeof(device_id); i++)
    {
        //sprintf(hex_device_id + i * 2, "%02X", NRF_FICR->DEVICEID[i]);  //Fark not even sprintf in s310..
        uint8_t num = (&device_id)[i];
            
        uint8_t ret = num / 16;
        uint8_t remain = num % 16;

        hex_device_id[index++] = hex_table[ret];
        hex_device_id[index++] = hex_table[remain];
        
    }

    return true;
}

bool hble_hex_to_uint64_device_id(const char* hex_device_id, uint64_t* device_id)
{
    if(!hex_device_id || !device_id)
    {
        return false;
    }

    uint16_t hex_len = strlen(hex_device_id);
    if(hex_len != sizeof(uint64_t) * 2)
    {
        return false;
    }

    const char* hex_table = "0123456789ABCDEF";
    const uint8_t hex_table_len = strlen(hex_table);
    *device_id = 0;

    uint8_t index = 0;

    for(uint8_t i = 0; i < sizeof(uint64_t); i++)
    {
        uint8_t multiplier = 0;
        uint8_t remain = 0;
        

        for(uint8_t j = 0; j < hex_table_len; j++)
        {
            if(hex_table[j] == hex_device_id[i * 2])
            {
                multiplier = j;
            }

            if(hex_table[j] == hex_device_id[i * 2 + 1])
            {
                remain = j;
            }
        }

        uint64_t num = (multiplier << 4) + remain;
        *device_id += num << (i * 8);
    }

    return true;

}

void hble_params_init(char* device_name)
{
    // initialize GAP parameters
    {
        ble_gap_conn_sec_mode_t sec_mode;
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

        APP_OK(sd_ble_gap_device_name_set(&sec_mode, (const uint8_t* const)device_name, strlen(device_name)));

        ble_gap_conn_params_t gap_conn_params = {};
        gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
        gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
        gap_conn_params.slave_latency     = SLAVE_LATENCY;
        gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

        APP_OK(sd_ble_gap_ppcp_set(&gap_conn_params));
        APP_OK(sd_ble_gap_tx_power_set(TX_POWER_LEVEL));
    }

    //Device Info
    {
        ble_dis_init_t dis_init;
        memset(&dis_init, 0, sizeof(dis_init));

        ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, BLE_MANUFACTURER_NAME);
        ble_srv_ascii_to_utf8(&dis_init.model_num_str, BLE_MODEL_NUM);

        size_t hex_device_id_len = 0;
        uint64_t device_id = 0;
        memcpy(&device_id, NRF_FICR->DEVICEID, sizeof(device_id));
        hble_uint64_to_hex_device_id(device_id, NULL, &hex_device_id_len);

        PRINTS("Device id hex string array size: ");
        PRINT_HEX(&hex_device_id_len, sizeof(hex_device_id_len));
        PRINTS("\r\n");

        char hex_device_id[hex_device_id_len];
        hble_uint64_to_hex_device_id(device_id, hex_device_id, &hex_device_id_len);

        ble_srv_ascii_to_utf8(&dis_init.serial_num_str, hex_device_id);
        

        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&dis_init.dis_attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dis_init.dis_attr_md.write_perm);

        APP_OK(ble_dis_init(&dis_init));
    }

#ifdef HAS_BATTERY_SERVICE
    //Battery level service
    {
        ble_bas_init_t bas_init;
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_char_attr_md.cccd_write_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_char_attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&bas_init.battery_level_char_attr_md.write_perm);

        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_report_read_perm);

        bas_init.evt_handler          = NULL;
        bas_init.support_notification = true;
        bas_init.p_report_ref         = NULL;
        bas_init.initial_batt_level   = 100;

        APP_OK(ble_bas_init(&_ble_bas, &bas_init));
    }
#endif

    // Connection parameters.
    {
        ble_conn_params_init_t cp_init = {};
        cp_init.p_conn_params                  = NULL;
        cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
        cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
        cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
        cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
        cp_init.disconnect_on_fail             = false;
        cp_init.evt_handler                    = _on_conn_params_evt;
        cp_init.error_handler                  = _on_conn_params_error;

        APP_OK(ble_conn_params_init(&cp_init));
    }

    // Sec params.
    {
        _sec_params.timeout      = SEC_PARAM_TIMEOUT;
        _sec_params.bond         = SEC_PARAM_BOND;
        _sec_params.mitm         = SEC_PARAM_MITM;
        _sec_params.io_caps      = SEC_PARAM_IO_CAPABILITIES;
        _sec_params.oob          = SEC_PARAM_OOB;
        _sec_params.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
        _sec_params.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
    }

}


void hble_services_init(void)
{
    // 1. Initialize HELLO UUID
    hlo_ble_init();  

    // 2. Add service
    ble_uuid_t pill_service_uuid = {
        .type = hello_type,
        .uuid = BLE_UUID_MORPHEUS_SVC ,
    };

    APP_OK(sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &pill_service_uuid, &_morpheus_service_handle));

    // 3. Initialize Morpheus' BLE transmission layer
    morpheus_ble_transmission_layer_init();

    // 4. Add characteristics, attach them to transmission layer
    hlo_ble_char_write_request_add(0xBEEB, &morpheus_ble_write_handler, sizeof(struct hlo_ble_packet));
    hlo_ble_char_notify_add(0xB00B);

    hlo_ble_char_notify_add(0xFEE1);
    
    hlo_ble_char_notify_add(BLE_UUID_DAY_DATE_TIME_CHAR);

    
    
}
