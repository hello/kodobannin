// vi:sw=4:ts=4

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
#include "device_params.h"
#include <ble_radio_notification.h>
#include <ble_flash.h>

extern void ble_bond_manager_init(void);
extern void ble_gap_params_init(void);
extern void ble_advertising_init(void);
extern void ble_advertising_start(void);
extern void ble_services_init(void);
extern void ble_conn_params_init(void);

extern void services_init();

static ble_gap_sec_params_t sec_params;

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;   /**< Handle of the current connection. */

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
#ifdef CONNECTED_LED_PIN_NO
		nrf_gpio_pin_set(CONNECTED_LED_PIN_NO);
#endif
#ifdef ADVERTISING_LED_PIN_NO
		nrf_gpio_pin_clear(ADVERTISING_LED_PIN_NO);
#endif
		break;

	case BLE_GAP_EVT_DISCONNECTED:
#ifdef CONNECTED_LED_PIN_NO
		nrf_gpio_pin_clear(CONNECTED_LED_PIN_NO);
#endif

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
#ifdef ADVERTISING_LED_PIN_NO
			nrf_gpio_pin_clear(ADVERTISING_LED_PIN_NO);
#endif
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

/**@brief BLE stack initialization.
*
* @details Initializes the SoftDevice and the BLE event interrupt.
*/
static void
ble_stack_init(void)
{
	BLE_STACK_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_SYNTH_250_PPM,
				BLE_L2CAP_MTU_DEF,
				ble_event_dispatch,
				false);
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
ble_sec_params_init(void)
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
ble_gap_params_init(void)
{
	uint32_t                err_code;
	ble_gap_conn_params_t   gap_conn_params;
	ble_gap_conn_sec_mode_t sec_mode;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

	err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)BLE_DEVICE_NAME, strlen(BLE_DEVICE_NAME));
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

#ifdef ADVERTISING_LED_PIN_NO
	GPIO_LED_CONFIG(ADVERTISING_LED_PIN_NO);
#endif
#ifdef CONNECTED_LED_PIN_NO
	GPIO_LED_CONFIG(CONNECTED_LED_PIN_NO);
#endif
#ifdef ASSERT_LED_PIN_NO
	GPIO_LED_CONFIG(ASSERT_LED_PIN_NO);
#endif
	ble_stack_init();
	ble_bond_manager_init();
	ble_gap_params_init();
	services_init();  // Must come before ble_advertising_init()
	ble_advertising_init();
	ble_services_init();
	ble_conn_params_init();
	ble_sec_params_init();

	err_code = ble_radio_notification_init(NRF_APP_PRIORITY_HIGH,
										   NRF_RADIO_NOTIFICATION_DISTANCE_4560US,
										   ble_flash_on_radio_active_evt);
	APP_ERROR_CHECK(err_code);
}
