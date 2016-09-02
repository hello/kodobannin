// vi:noet:sw=4 ts=4

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <app_timer.h>
#include <ble.h>
#include <ble_advdata.h>
#include <ble_conn_params.h>
#include <ble_dis.h>
#include <ble_bas.h>
#include <ble_hci.h>
#include <nrf_sdm.h>
#include <nrf_soc.h>
#include <pstorage.h>
#include <softdevice_handler.h>
#include <ble_bondmngr.h>

#include "app.h"
#include "platform.h"
#include "hble.h"
#include "util.h"
#include "pill_gatt.h"

#include "battery.h"
#include "app_info.h"
#include "ant_devices.h"

//static hble_evt_handler_t _user_ble_evt_handler;
//static uint16_t _connection_handle = BLE_CONN_HANDLE_INVALID;
static ble_gap_sec_params_t _sec_params;
static ble_bas_t  _ble_bas;
#ifdef BONDING_REQUIRED
static bool app_initialized = false;
#endif

static ble_uuid_t _service_uuid;
static int8_t  _last_connected_central; 

static void _on_disconnect(void * p_event_data, uint16_t event_size)
{
#ifdef BONDING_REQUIRED
    APP_OK(ble_bondmngr_bonded_centrals_store());
#endif

    hble_advertising_start();
}


static void _on_advertise_timeout(void * p_event_data, uint16_t event_size)
{
    //nrf_delay_ms(100);
    //hble_advertising_start();
}

static void _on_ble_evt(ble_evt_t* ble_evt)
{
    //static ble_gap_evt_auth_status_t _auth_status;

    switch(ble_evt->header.evt_id) {
    
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
    ble_bas_on_ble_evt(&_ble_bas, p_ble_evt);
    hlo_ble_on_ble_evt(p_ble_evt);
    
    _on_ble_evt(p_ble_evt);
    
}


static void _on_sys_evt(uint32_t sys_evt)
{
    PRINTS("_on_sys_evt: ");
    PRINT_HEX(&sys_evt, sizeof(sys_evt));
    PRINTS("\r\n");

    pstorage_sys_event_handler(sys_evt);
}

static void _on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    switch(p_evt->evt_type) {
    case BLE_CONN_PARAMS_EVT_FAILED:
        PRINTS("BLE_CONN_PARAMS_EVT_FAILED");
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
    _last_connected_central = p_evt->central_handle;
    PRINTS("last connected central set\r\n");
}


/**@brief Function for the Bond Manager initialization.
 */
void hble_bond_manager_init()
{
    ble_bondmngr_init_t bond_init_data;

    // Initialize persistent storage module.
    APP_OK(pstorage_init());
    //PRINTS("pstorage_init() done.\r\n");

    memset(&bond_init_data, 0, sizeof(bond_init_data));
    
    // Initialize the Bond Manager.
    bond_init_data.evt_handler             = _bond_evt_handler;
    bond_init_data.error_handler           = _bond_manager_error_handler;
    bond_init_data.bonds_delete            = true;

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
    ble_uuid_t adv_uuids[] =
    {
        _service_uuid,
        {BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE}
    };
	//manufacturing data
	ble_advdata_manuf_data_t manf = (ble_advdata_manuf_data_t){0};
	hlo_ble_adv_manuf_data_t hlo_manf = (hlo_ble_adv_manuf_data_t){
		.hw_type = HLO_ANT_DEVICE_TYPE_PILL1_5,
		.fw_version = FIRMWARE_VERSION_8BIT,
		.id = GET_UUID_64(),
	};
	manf.company_identifier = BLE_SIG_COMPANY_ID;
	manf.data.p_data = &hlo_manf;
	manf.data.size = sizeof(hlo_manf);
		
    // Build and set advertising data
    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = true;
    advdata.flags.size = sizeof(flags);
    advdata.flags.p_data = &flags;
	advdata.p_manuf_specific_data = &manf;
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

    if(app_initialized)                
    {
        // ble_gap_addr_t peer_address;
        //uint32_t err_code = ble_bondmngr_central_addr_get(_last_connected_central, &peer_address));
        //uint8_t adv_mode = (err_code == NRF_SUCCESS ? BLE_GAP_ADV_TYPE_ADV_DIRECT_IND : BLE_GAP_ADV_TYPE_ADV_IND);
        uint8_t adv_mode = BLE_GAP_ADV_TYPE_ADV_IND; 
        adv_params.type = adv_mode;

        switch(adv_mode)
        {
            case BLE_GAP_ADV_TYPE_ADV_DIRECT_IND:
                // Direct advertising, no used for now since no much device supports BLE 4.1
                // adv_params.timeout     = 0;
                // adv_params.p_peer_addr = &peer_address;
                break;

            case BLE_GAP_ADV_TYPE_ADV_IND:
                {
                    
                    ble_gap_whitelist_t whitelist;
                    APP_OK(ble_bondmngr_whitelist_get(&whitelist));
                    
                    
                    if (whitelist.addr_count != 0 || whitelist.irk_count != 0)
                    {
                        adv_params.p_whitelist = &whitelist;
                        adv_params.fp          = BLE_GAP_ADV_FP_FILTER_BOTH;
                        flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

                        PRINTS("whitelist retrieved.\r\n");
                    }else{
                        flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;  // Just make it clear what we want to do.
                        PRINTS("NO whitelist retrieved.\r\n");
                    }
                }
                break;
        } 
        
    }
    else
    {
        flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;  // Just to make things clear
        app_initialized = true;
    }

#endif

    _advertising_data_init(flags);
    sd_ble_gap_adv_start(&adv_params);

    PRINTS("Advertising started.\r\n");
}

void hble_stack_init()
{
    // Register with the SoftDevice handler module for BLE events.
    APP_OK(softdevice_ble_evt_handler_set(_ble_evt_dispatch));

    // Register with the SoftDevice handler module for BLE events.
    APP_OK(softdevice_sys_evt_handler_set(_on_sys_evt));

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

        size_t device_id_len = sizeof(NRF_FICR->DEVICEID); 

        PRINTS("Device id array size: ");
        PRINT_HEX(&device_id_len, sizeof(device_id_len));
        PRINTS("\r\n");

        size_t hex_device_id_len = device_id_len * 2 + 1;
        char hex_device_id[hex_device_id_len];
        char device_id[device_id_len];

        memcpy(device_id, (const uint8_t*)NRF_FICR->DEVICEID, device_id_len);
        memset(hex_device_id, 0, hex_device_id_len);
        const char* hex_table = "0123456789ABCDEF";
        
        size_t index = 0;
        for(size_t i = 0; i < device_id_len; i++)
        {
            //sprintf(hex_device_id + i * 2, "%02X", NRF_FICR->DEVICEID[i]);  //Fark not even sprintf in s310..
            uint8_t num = device_id[i];
                
            uint8_t ret = num / 16;
            uint8_t remain = num % 16;

            hex_device_id[index++] = hex_table[ret];
            hex_device_id[index++] = hex_table[remain];
            
        }

        ble_srv_ascii_to_utf8(&dis_init.serial_num_str, hex_device_id);
        

        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&dis_init.dis_attr_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dis_init.dis_attr_md.write_perm);

        APP_OK(ble_dis_init(&dis_init));
    }

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
