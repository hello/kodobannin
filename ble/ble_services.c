#include <ble_types.h>
#include <ble_bas.h>
#include <ble_dis.h>
#include <string.h>
#include <device_params.h>

static ble_bas_t  m_bas; /**< Structure used to identify the battery service. */

/**@brief Initialize services that will be used by the application.
 *
 * @details Initialize the Blood Pressure, Battery and Device Information services.
 */
void
ble_services_init(void)
{
	uint32_t         err_code;
	ble_bas_init_t   bas_init;
	ble_dis_init_t   dis_init;
	ble_dis_sys_id_t sys_id;

	// Initialize Battery Service
	memset(&bas_init, 0, sizeof(bas_init));

	// Here the sec level for the Battery Service can be changed/increased.
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_char_attr_md.cccd_write_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_char_attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&bas_init.battery_level_char_attr_md.write_perm);

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_report_read_perm);

	bas_init.evt_handler          = NULL;
	bas_init.support_notification = true;
	bas_init.p_report_ref         = NULL;
	bas_init.initial_batt_level   = 100;

	err_code = ble_bas_init(&m_bas, &bas_init);
	APP_ERROR_CHECK(err_code);

	// Initialize Device Information Service
	memset(&dis_init, 0, sizeof(dis_init));

	ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, BLE_MANUFACTURER_NAME);
	ble_srv_ascii_to_utf8(&dis_init.model_num_str,     BLE_MODEL_NUM);

	sys_id.manufacturer_id            = BLE_MANUFACTURER_ID;
	sys_id.organizationally_unique_id = BLE_ORG_UNIQUE_ID;
	dis_init.p_sys_id                 = &sys_id;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&dis_init.dis_attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dis_init.dis_attr_md.write_perm);

	err_code = ble_dis_init(&dis_init);
	APP_ERROR_CHECK(err_code);
}

uint32_t
ble_services_update_battery_level(uint8_t level)
{
	return ble_bas_battery_level_update(&m_bas, level);
}