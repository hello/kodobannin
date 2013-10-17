/* Copyright (c) 2013 Hello Inc. All Rights Reserved. */

#include "ble_hello_demo.h"

#include <stdlib.h>
#include <string.h>
#include <app_error.h>
#include <ble_gatts.h>
#include <nordic_common.h>
#include <ble_srv_common.h>
#include <app_util.h>
#include <util.h>
#include <ble.h>

static uint8_t hello_type;
static uint16_t                 service_handle;
static uint16_t                 conn_handle;
static ble_gatts_char_handles_t sys_mode_handles;
static ble_gatts_char_handles_t sys_data_handles;
static ble_hello_demo_write_handler    data_write_handler;
static ble_hello_demo_write_handler    mode_write_handler;
static ble_hello_demo_connect_handler  conn_handler;
static ble_hello_demo_connect_handler  disconn_handler;

static void
default_write_handler(ble_gatts_evt_write_t *event) {
    DEBUG("Default write handler called for 0x", event->handle);
}

static void
default_conn_handler(void) {
    PRINTS("Default conn handler called");
}

static void
dispatch_write(ble_evt_t *event) {
    ble_gatts_evt_write_t *write_evt = &event->evt.gatts_evt.params.write;
    uint16_t handle = write_evt->handle;
    DEBUG("write, handle: 0x", handle);
    DEBUG("data, handle: 0x", sys_data_handles.cccd_handle);
    DEBUG("mode, handle: 0x", sys_mode_handles.value_handle);
    if (handle == sys_data_handles.cccd_handle) {
        data_write_handler(write_evt);
    } else if (handle == sys_mode_handles.value_handle) {
        mode_write_handler(write_evt);
    } else {
        DEBUG("Unhandled write to handle 0x", handle);
    }
}

void
ble_hello_demo_on_ble_evt(ble_evt_t *event)
{
    DEBUG("event id: 0x", event->header.evt_id)
    switch (event->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            conn_handle = event->evt.gap_evt.conn_handle;
            conn_handler();
            break;
            
        case BLE_GAP_EVT_DISCONNECTED:
            conn_handle = BLE_CONN_HANDLE_INVALID;
            disconn_handler();
            break;
            
        case BLE_GATTS_EVT_WRITE:
            dispatch_write(event);
            break;
            
        default:
            DEBUG("Hello Demo event unhandled: 0x", event->header.evt_id);
            break;
    };
}

uint16_t
ble_hello_demo_data_send(const uint8_t *data, const uint16_t len) {
    uint32_t err_code;
    uint16_t hvx_len;
    ble_gatts_hvx_params_t hvx_params;

    //todo: return verbose errors instead of 0
    if (conn_handle == BLE_CONN_HANDLE_INVALID)
        return 0;

    hvx_len = len;

    memset(&hvx_params, 0, sizeof(hvx_params));
    
    hvx_params.handle   = sys_data_handles.value_handle;
    hvx_params.type     = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset   = 0;
    hvx_params.p_len    = &hvx_len;
    hvx_params.p_data   = (uint8_t *)data;
    
    err_code = sd_ble_gatts_hvx(conn_handle, &hvx_params);
    if (err_code == NRF_SUCCESS)
        return hvx_len;
    else {
        DEBUG("Hello Demo data send err 0x", err_code);
    }
    return 0;
}
/*
static void
on_data_fetch_cccd_write(ble_gatts_evt_write_t * p_evt_write)
{
    if (p_evt_write->len == 2)
    {
        // CCCD written, update notification state
        if (p_dts->evt_handler != NULL)
        {
            ble_dts_evt_t evt;
            
            if (ble_srv_is_notification_enabled(p_evt_write->data))
            {
                evt.evt_type = BLE_DTS_EVT_NOTIFICATION_ENABLED;
            }
            else
            {
                evt.evt_type = BLE_DTS_EVT_NOTIFICATION_DISABLED;
            }
            
            p_dts->evt_handler(p_dts, &evt);
        }
    }
}
*/

/**@brief Function for adding the Characteristic.
 *
 * @param[in]   uuid           UUID of characteristic to be added.
 * @param[in]   p_char_value   Initial value of characteristic to be added.
 * @param[in]   char_len       Length of initial value. This will also be the maximum value.
 * @param[in]   dis_attr_md    Security settings of characteristic to be added.
 * @param[out]  p_handles      Handles of new characteristic.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t
char_add(uint16_t                        uuid,
         bool                            disable_write,
         uint8_t *                       p_char_value,
         uint16_t                        char_len,
         ble_gatts_char_handles_t *      p_handles)
{
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    
    memset(&cccd_md, 0, sizeof(cccd_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);// = p_dts_init->dts_data_fetch_attr_md.cccd_write_perm;
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;

    memset(&char_md, 0, sizeof(char_md));
    
    char_md.char_props.notify       = 1;
    char_md.p_char_user_desc        = NULL;
    char_md.p_char_pf               = NULL;
    char_md.p_user_desc_md          = NULL;
    char_md.p_cccd_md               = &cccd_md;
    char_md.p_sccd_md               = NULL;
    
    BLE_UUID_BLE_ASSIGN(ble_uuid, uuid);
    
    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);//  = p_dts_init->dts_data_fetch_attr_md.read_perm;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);// = p_dts_init->dts_data_fetch_attr_md.write_perm;
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 1;
    
    memset(&attr_char_value, 0, sizeof(attr_char_value));
    
    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = char_len;
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = char_len;
    attr_char_value.p_value      = p_char_value;
    
    return sd_ble_gatts_characteristic_add(service_handle,
                                        &char_md,
                                        &attr_char_value,
                                        p_handles);
    /*ble_uuid_t          ble_uuid;
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t attr_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    
    APP_ERROR_CHECK_BOOL(p_char_value != NULL);
    APP_ERROR_CHECK_BOOL(char_len > 0);
    
    // Setup cccd security modes
    memset(&cccd_md, 0, sizeof(cccd_md));
    memset(&char_md, 0, sizeof(char_md));
    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;

    // disabe writes
    if (disable_write) {
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
        
    } else {
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    }

    // The ble_gatts_char_md_t structure uses bit fields. So we reset the memory to zero.
    //char_md.char_props.read   = 1;
    char_md.char_props.notify = 1;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = &cccd_md;
    char_md.p_sccd_md         = NULL;

    BLE_UUID_BLE_ASSIGN(ble_uuid, uuid);
    
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 0;
    
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = char_len;
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = char_len;
    attr_char_value.p_value      = p_char_value;

    return sd_ble_gatts_characteristic_add(service_handle, &char_md, &attr_char_value, p_handles);
*/}

static uint32_t
char_add2(uint16_t                        uuid,
         bool                            disable_write,
         uint8_t *                       p_char_value,
         uint16_t                        char_len,
         ble_gatts_char_handles_t *      p_handles) {
    ble_gatts_char_md_t char_md;
    //ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    /*
    memset(&cccd_md, 0, sizeof(cccd_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);// = p_dts_init->dts_data_fetch_attr_md.cccd_write_perm;
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;
*/
    memset(&char_md, 0, sizeof(char_md));
    
    //char_md.char_props.notify       = 1;
    char_md.char_props.read         = 1;
    char_md.char_props.write        = 1;
    char_md.p_char_user_desc        = NULL;
    char_md.p_char_pf               = NULL;
    char_md.p_user_desc_md          = NULL;
    char_md.p_cccd_md               = NULL; //&cccd_md;
    char_md.p_sccd_md               = NULL;
    
    BLE_UUID_BLE_ASSIGN(ble_uuid, uuid);
    
    memset(&attr_md, 0, sizeof(attr_md));

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);//  = p_dts_init->dts_data_fetch_attr_md.read_perm;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);// = p_dts_init->dts_data_fetch_attr_md.write_perm;
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 1;
    
    memset(&attr_char_value, 0, sizeof(attr_char_value));
    
    attr_char_value.p_uuid       = &ble_uuid;
    attr_char_value.p_attr_md    = &attr_md;
    attr_char_value.init_len     = char_len;
    attr_char_value.init_offs    = 0;
    attr_char_value.max_len      = char_len;
    attr_char_value.p_value      = p_char_value;
    
    return sd_ble_gatts_characteristic_add(service_handle,
                                        &char_md,
                                        &attr_char_value,
                                        p_handles);
}

uint32_t ble_hello_demo_init(const ble_hello_demo_init_t *init) {
    uint32_t   err_code;
    ble_uuid_t ble_uuid;
    const ble_uuid128_t hello_uuid = {.uuid128 = BLE_UUID_HELLO_BASE};
    uint8_t zeroes[20] = {0xAC};

    // Init defaults
    conn_handle = BLE_CONN_HANDLE_INVALID;
    data_write_handler = &default_write_handler;
    mode_write_handler = &default_write_handler;
    conn_handler       = &default_conn_handler;
    disconn_handler    = &default_conn_handler;

    // Assign write handlers
    if (init->data_write_handler)
        data_write_handler = init->data_write_handler;

    if (init->mode_write_handler)
        mode_write_handler = init->mode_write_handler;

    if (init->conn_handler)
        conn_handler = init->conn_handler;

    if (init->disconn_handler)
        disconn_handler = init->disconn_handler;

    // Add Hello's Base UUID
    err_code = sd_ble_uuid_vs_add(&hello_uuid, &hello_type);
    if (err_code != NRF_SUCCESS)
        return err_code;

    // Add service
    ble_uuid.type = hello_type;
    ble_uuid.uuid = BLE_UUID_HELLO_DEMO_SVC;

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &service_handle);
    if (err_code != NRF_SUCCESS)
        return err_code;

    // Add characteristics
    err_code = char_add(BLE_UUID_DATA_CHAR,
                        0,
                        zeroes, 
                        BLE_GAP_DEVNAME_MAX_WR_LEN,
                        &sys_data_handles);
    if (err_code != NRF_SUCCESS)
        return err_code;
    
    err_code = char_add2(BLE_UUID_CONF_CHAR,
                        0,
                        zeroes,
                        4,
                        &sys_mode_handles);
    if (err_code != NRF_SUCCESS)
        return err_code;

    return NRF_SUCCESS;
}
