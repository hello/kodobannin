#include <app_error.h>
#include <ble_types.h>
#include <ble_gap.h>
#include <string.h>
#include "device_params.h"

#define SEC_PARAM_TIMEOUT          30                    /**< Timeout for Pairing Request or Security Request (in seconds). */
#define SEC_PARAM_BOND             1                     /**< Perform bonding. */
#define SEC_PARAM_MITM             0                     /**< Man In The Middle protection not required. */
#define SEC_PARAM_IO_CAPABILITIES  BLE_GAP_IO_CAPS_NONE  /**< No I/O capabilities. */
#define SEC_PARAM_OOB              0                     /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE     7                     /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE     16                    /**< Maximum encryption key size. */

#define SLAVE_LATENCY                        0                                          /**< Slave latency. */

ble_gap_sec_params_t sec_params;

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
