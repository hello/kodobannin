// vi:noet:sw=4 ts=4

#include <ble_advdata.h>
#include "hlo_ble_demo.h"
#include <ble_gap.h>
#include <ble_hci.h>
#include <ble_l2cap.h>
#include <nrf_gpio.h>
#include <nrf_sdm.h>
#include <ble_types.h>
#include <string.h>
#include <ble_bondmngr.h>
#include "ble_event.h"
#include <ble_stack_handler.h>
#include "platform.h"
#include <ble_radio_notification.h>
#include <ble_flash.h>
#include <ble_conn_params.h>

#include "ble_core.h"
#include "util.h"
#include "platform.h"

extern void ble_bond_manager_init(void);
extern void ble_services_init(void);

extern void services_init();

static ble_gap_sec_params_t sec_params;

ble_gap_sec_params_t* ble_gap_sec_params_get()
{
	return &sec_params;
}

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;   /**< Handle of the current connection. */

static ble_gap_adv_params_t m_adv_params;

extern uint8_t hello_type;

void
ble_advertising_init(void)
{
	uint32_t      err_code;
	ble_advdata_t advdata;
	uint8_t	      flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

	// Build and set advertising data
	memset(&advdata, 0, sizeof(advdata));

	advdata.name_type               = BLE_ADVDATA_FULL_NAME;
	advdata.include_appearance      = true;
	advdata.flags.size              = sizeof(flags);
	advdata.flags.p_data            = &flags;

	// Scan response packet
	ble_uuid_t sr_uuid = {
		.uuid = BLE_UUID_HELLO_DEMO_SVC,
		.type = hello_type
	};

	ble_advdata_t srdata;
	memset(&srdata, 0, sizeof(srdata));
	srdata = (ble_advdata_t) {
		.uuids_more_available.uuid_cnt = 1,
		.uuids_more_available.p_uuids = &sr_uuid,
	};

	err_code = ble_advdata_set(&advdata, &srdata);
	APP_ERROR_CHECK(err_code);

	// Initialise advertising parameters (used when starting advertising)
	memset(&m_adv_params, 0, sizeof(m_adv_params));

	m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_IND;
	m_adv_params.p_peer_addr = NULL; // Undirected advertisement
	m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
	m_adv_params.interval    = APP_ADV_INTERVAL;
	m_adv_params.timeout     = APP_ADV_TIMEOUT_IN_SECONDS;
}

void
ble_advertising_start(void)
{
	uint32_t err_code;

	err_code = sd_ble_gap_adv_start(&m_adv_params);
	APP_ERROR_CHECK(err_code);
}

/**@brief Application's BLE Stack event handler.
*
* @param[in]   p_ble_evt   Bluetooth stack event.
*/
void
on_ble_event(ble_evt_t *p_ble_evt)
{
	uint32_t err_code = NRF_SUCCESS;

	switch (p_ble_evt->header.evt_id)
	{
	case BLE_GAP_EVT_CONNECTED:
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		m_conn_handle               = BLE_CONN_HANDLE_INVALID;

		// Since we are not in a connection and have not started advertising, store bonds
		err_code = ble_bondmngr_bonded_masters_store();
		APP_ERROR_CHECK(err_code);

		ble_advertising_start();
		break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		err_code = sd_ble_gap_sec_params_reply(m_conn_handle,
			BLE_GAP_SEC_STATUS_SUCCESS,
			&sec_params);
		break;

	case BLE_GAP_EVT_TIMEOUT:
		if (p_ble_evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_ADVERTISEMENT) {
			//while(1) { __WFE(); }
			// Go to system-off mode (this function will not return; wakeup will cause a reset)
			// why would we want this?
			//err_code = sd_power_system_off();
			//re-start advertising again instead
			ble_advertising_start();
		}
		break;

	case BLE_GATTS_EVT_TIMEOUT:
		if (p_ble_evt->evt.gatts_evt.params.timeout.src == BLE_GATT_TIMEOUT_SRC_PROTOCOL) {
			err_code = sd_ble_gap_disconnect(m_conn_handle,
				BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		}
		break;

	default:
		break;
	}

	APP_ERROR_CHECK(err_code);
}

#define SEC_PARAM_TIMEOUT          30                    /**< Timeout for Pairing Request or Security Request (in seconds). */
#define SEC_PARAM_BOND             1                     /**< Perform bonding. */
#define SEC_PARAM_MITM             0                     /**< Man In The Middle protection not required. */
#define SEC_PARAM_IO_CAPABILITIES  BLE_GAP_IO_CAPS_NONE  /**< No I/O capabilities. */
#define SEC_PARAM_OOB              0                     /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE     7                     /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE     16                    /**< Maximum encryption key size. */

#define SLAVE_LATENCY                        0                                          /**< Slave latency. */

/**@brief Initialize security parameters.
 */
void
ble_gap_sec_params_init(void)
{
	memset(&sec_params, 0, sizeof(sec_params));

	sec_params.timeout      = SEC_PARAM_TIMEOUT;
	sec_params.bond         = SEC_PARAM_BOND;
	sec_params.mitm         = SEC_PARAM_MITM;
	sec_params.io_caps      = SEC_PARAM_IO_CAPABILITIES;
	sec_params.oob          = SEC_PARAM_OOB;
	sec_params.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
	sec_params.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
}

/**@brief GAP initialization.
 *
 * @details This function shall be used to setup all the necessary GAP (Generic Access Profile)
 *          parameters of the device. It also sets the permissions and appearance.
 */
void
ble_gap_params_init(char* device_name)
{
	uint32_t                err_code;
	ble_gap_conn_params_t   gap_conn_params;
	ble_gap_conn_sec_mode_t sec_mode;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

	err_code = sd_ble_gap_device_name_set(&sec_mode, (uint8_t*)device_name, strlen(device_name));
	APP_ERROR_CHECK(err_code);

/*
	err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_BLOOD_PRESSURE);
	APP_ERROR_CHECK(err_code);
*/
	memset(&gap_conn_params, 0, sizeof(gap_conn_params));

	gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
	gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
	gap_conn_params.slave_latency     = SLAVE_LATENCY;
	gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

	err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
	APP_ERROR_CHECK(err_code);
}

void
ble_init()
{
	uint32_t err_code;

	// append something to device name
    char device_name[strlen(BLE_DEVICE_NAME)+4];

    memcpy(device_name, BLE_DEVICE_NAME, strlen(BLE_DEVICE_NAME));

	uint8_t id = *(uint8_t *)NRF_FICR->DEVICEID;
	//DEBUG("ID is ", id);
	device_name[strlen(BLE_DEVICE_NAME)] = '-';
	device_name[strlen(BLE_DEVICE_NAME)+1] = hex[(id >> 4) & 0xF];
	device_name[strlen(BLE_DEVICE_NAME)+2] = hex[(id & 0xF)];
	device_name[strlen(BLE_DEVICE_NAME)+3] = '\0';

	BLE_STACK_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_SYNTH_250_PPM,
                           BLE_L2CAP_MTU_DEF,
				ble_event_dispatch,
				false);

	ble_bond_manager_init();
	ble_gap_params_init(device_name);
	services_init();  // Must come before ble_advertising_init()
	ble_advertising_init();
	ble_services_init();
	ble_conn_params_init(NULL);
	ble_gap_sec_params_init();

	err_code = ble_radio_notification_init(NRF_APP_PRIORITY_HIGH,
										   NRF_RADIO_NOTIFICATION_DISTANCE_4560US,
										   ble_flash_on_radio_active_evt);
	APP_ERROR_CHECK(err_code);
}
