#include <ble_advdata.h>
#include <ble_srv_common.h>
#include <ble_types.h>
#include <nrf_gpio.h>
#include "device_params.h"

static ble_gap_adv_params_t m_adv_params;

/**@brief Advertising functionality initialization.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *		  Also builds a structure to be passed to the stack when starting advertising.
 */
void
ble_advertising_init(void)
{
	uint32_t      err_code;
	ble_advdata_t advdata;
	uint8_t	      flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

	ble_uuid_t adv_uuids[] =
	{
		//{BLE_UUID_BLOOD_PRESSURE_SERVICE,     BLE_UUID_TYPE_BLE},
		{BLE_UUID_BATTERY_SERVICE,            BLE_UUID_TYPE_BLE},
		{BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}
	};

	// Build and set advertising data
	memset(&advdata, 0, sizeof(advdata));

	advdata.name_type               = BLE_ADVDATA_FULL_NAME;
	advdata.include_appearance      = true;
	advdata.flags.size              = sizeof(flags);
	advdata.flags.p_data            = &flags;
	advdata.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
	advdata.uuids_complete.p_uuids  = adv_uuids;

	err_code = ble_advdata_set(&advdata, NULL);
	APP_ERROR_CHECK(err_code);

	// Initialise advertising parameters (used when starting advertising)
	memset(&m_adv_params, 0, sizeof(m_adv_params));

	m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_IND;
	m_adv_params.p_peer_addr = NULL; // Undirected advertisement
	m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
	m_adv_params.interval    = APP_ADV_INTERVAL;
	m_adv_params.timeout     = APP_ADV_TIMEOUT_IN_SECONDS;
}

/**@brief Start advertising.
 */
void
ble_advertising_start(void)
{
	uint32_t err_code;

	err_code = sd_ble_gap_adv_start(&m_adv_params);
	APP_ERROR_CHECK(err_code);

#ifdef ADVERTISING_LED_PIN_NO
	nrf_gpio_pin_set(ADVERTISING_LED_PIN_NO);
#endif
}

