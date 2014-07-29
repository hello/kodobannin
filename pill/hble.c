// vi:noet:sw=4 ts=4

#include <stdlib.h>
#include <string.h>

#include <app_timer.h>
#include <ble.h>
#include <ble_advdata.h>
#include <ble_conn_params.h>
#include <ble_hci.h>
#include <nrf_sdm.h>
#include <nrf_soc.h>
#include <pstorage.h>
#include <softdevice_handler.h>
#include <ble_bondmngr.h>

#include "app.h"
#include "hble.h"
#include "util.h"
#include "pill_gatt.h"

//static hble_evt_handler_t _user_ble_evt_handler;
//static uint16_t _connection_handle = BLE_CONN_HANDLE_INVALID;
static ble_gap_sec_params_t _sec_params;

static bool no_advertising = false;
static bool _app_initialized = false;

static ble_uuid_t _service_uuid;
static int8_t  _last_connected_central; 

//static ble_gap_addr_t _whitelist_central_addr;
//static ble_gap_irk_t _whitelist_irk;

//static ble_gap_whitelist_t  _whitelist;
//static ble_gap_addr_t* p_whitelist_addr = { &_whitelist_central_addr };
//static ble_gap_irk_t* p_whitelist_irk = { &_whitelist_irk };

static void _on_disconnect(void * p_event_data, uint16_t event_size)
{
    APP_OK(ble_bondmngr_bonded_centrals_store());
    hble_advertising_start();
}

static void _on_ble_evt(ble_evt_t* ble_evt)
{
    //static ble_gap_evt_auth_status_t _auth_status;

    switch(ble_evt->header.evt_id) {
    
    case BLE_GAP_EVT_DISCONNECTED:
        //_connection_handle = BLE_CONN_HANDLE_INVALID;
        //APP_OK(ble_bondmngr_bonded_centrals_store());
        app_sched_event_put(NULL, 0, _on_disconnect);
        
       
        break;
    case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
        APP_OK(sd_ble_gap_sec_params_reply(hlo_ble_get_connection_handle(),
                                       BLE_GAP_SEC_STATUS_SUCCESS,
                                       &_sec_params));
        break;
    case BLE_GATTS_EVT_SYS_ATTR_MISSING:
        APP_OK(sd_ble_gatts_sys_attr_set(hlo_ble_get_connection_handle(), NULL, 0));
        break;
    /* 
    case BLE_GAP_EVT_AUTH_STATUS:
        _auth_status = ble_evt->evt.gap_evt.params.auth_status;

        //if(_auth_status.auth_status == BLE_GAP_SEC_STATUS_SUCCESS)
        {
            _whitelist_central_addr = _central_addr;
            //p_whitelist_addr[0] = &_whitelist_central_addr;

            _whitelist_irk = _auth_status.central_keys.irk;
            //p_whitelist_irk[0] = &_whitelist_irk;
            _whitelist.addr_count = 1;
            _whitelist.irk_count = 1;
            _whitelist.pp_addrs = p_whitelist_addr;
            _whitelist.pp_irks = p_whitelist_irk;

            PRINTS("paired");
        }
        break;
    */
    /* 
    case BLE_GAP_EVT_SEC_INFO_REQUEST:
        {
            ble_gap_enc_info_t* p_enc_info = &_auth_status.periph_keys.enc_info;
            if(p_enc_info->div == ble_evt->evt.gap_evt.params.sec_info_request.div) {
                APP_OK(sd_ble_gap_sec_info_reply(_connection_handle, p_enc_info, NULL));
            } else {
                // No keys found for this device
                APP_OK(sd_ble_gap_sec_info_reply(_connection_handle, NULL, NULL));
            }
        }
        break;
    */

    case BLE_GAP_EVT_TIMEOUT:
        if (ble_evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_ADVERTISEMENT) {
            // APP_OK(sd_power_system_off());
            hble_advertising_start();
        }
        break;
    default:
        // No implementation needed.
        break;
    }
    
}


static void _ble_evt_dispatch(ble_evt_t* p_ble_evt)
{
    ble_bondmngr_on_ble_evt(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    hlo_ble_on_ble_evt(p_ble_evt);
    
    _on_ble_evt(p_ble_evt);
    
}


static void _on_sys_evt(uint32_t sys_evt)
{
    uint32_t err_code;
    uint32_t count;
    pstorage_sys_event_handler(sys_evt);
    
    /*if(_app_initialized)
    {
        err_code = pstorage_access_status_get(&count);
        if(err_code == NRF_SUCCESS && count == 0)
        {
            hble_advertising_start();
            PRINTS("Adv triggered by sys event.\r\n");
        }
    }else{
        _app_initialized = true;
    }*/
    

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
    uint32_t            err_code;
    ble_bondmngr_init_t bond_init_data;
    bool                bonds_delete;

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



static void
_on_conn_params_error(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static void _advertising_data_init(uint8_t flags){
    ble_advdata_t advdata;
    ble_advdata_t scanrsp;
    ble_uuid_t adv_uuids[] = {_service_uuid};
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

    if(no_advertising)                
    {
        ble_gap_addr_t peer_address;
        //APP_OK(ble_bondmngr_central_addr_get(_last_connected_central, &peer_address));
        uint8_t adv_mode = BLE_GAP_ADV_TYPE_ADV_IND; //(err_code == NRF_SUCCESS ? BLE_GAP_ADV_TYPE_ADV_DIRECT_IND : BLE_GAP_ADV_TYPE_ADV_IND);
        adv_params.type        = adv_mode;

        switch(adv_mode)
        {
            case BLE_GAP_ADV_TYPE_ADV_DIRECT_IND:
                
                adv_params.timeout     = 0;
                adv_params.p_peer_addr = &peer_address;
                break;

            case BLE_GAP_ADV_TYPE_ADV_IND:
                {
                    
                    ble_gap_whitelist_t whitelist;
                    APP_OK(ble_bondmngr_whitelist_get(&whitelist));
                    

                    if (whitelist.addr_count != 0 || whitelist.irk_count != 0)
                    {
                        adv_params.p_whitelist = &whitelist;
                        adv_params.fp          = BLE_GAP_ADV_FP_FILTER_BOTH;
                        PRINTS("whitelist retrieved.\r\n");
                    }
                    
                    PRINTS("NO whitelist retrieved.\r\n");
                    
                    //if(_whitelist.addr_count != 0 || _whitelist.irk_count != 0)
                    //{
                    //    adv_params.p_whitelist = &_whitelist;
                    //    adv_params.fp          = BLE_GAP_ADV_FP_FILTER_CONNREQ;
                    //}
                    
                }
                break;
        }
        
        //_advertising_data_init(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    }
    else
    {
        _advertising_data_init(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
        no_advertising = true;
    }

    APP_OK(sd_ble_gap_adv_start(&adv_params));
    PRINTS("Advertising started.\r\n");
}

void hble_stack_init(nrf_clock_lfclksrc_t clock_source, bool use_scheduler)
{
    uint32_t err_code;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(clock_source, use_scheduler);

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
    }

    // initialize advertising parameters
    {
        ble_uuid_t adv_uuids[] = {{BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE}};
        uint8_t flags = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

        ble_advdata_t advdata = {};
        advdata.name_type = BLE_ADVDATA_FULL_NAME;
        advdata.include_appearance = true;
        advdata.flags.size = sizeof(flags);
        advdata.flags.p_data = &flags;
        advdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
        advdata.uuids_complete.p_uuids = adv_uuids;

        APP_OK(ble_advdata_set(&advdata, NULL));
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
