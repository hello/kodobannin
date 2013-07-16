#include <ble_bondmngr.h>
#include "device_params.h"

/**@brief Bond Manager module error handler.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void
ble_bond_manager_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Bond Manager module event handler.
 *
 * @param[in]   p_evt   Data associated to the bond manager event.
 */
void
ble_bond_manager_event_handler(ble_bondmngr_evt_t * p_evt)
{
    //uint32_t err_code;
    //bool     is_indication_enabled;

    switch (p_evt->evt_type)
    {
        case BLE_BONDMNGR_EVT_ENCRYPTED:
            break;

        case BLE_BONDMNGR_EVT_CONN_TO_BONDED_MASTER:
            // Send a single blood pressure measurement if indication is enabled.
            // NOTE: For this to work, make sure ble_bps_on_ble_evt() is called before
            //       ble_bondmngr_on_ble_evt() in ble_evt_dispatch().
            /*err_code = ble_bps_is_indication_enabled(&m_bps, &is_indication_enabled);
            APP_ERROR_CHECK(err_code);
            if (is_indication_enabled)
            {
                blood_pressure_measurement_send();
            }*/
            break;
        default:
            break;
    }
}

/**@brief Bond Manager initialization.
 */
void
ble_bond_manager_init(void)
{
	uint32_t            err_code;
	ble_bondmngr_init_t bond_init_data;
	bool                delete_bonds = false;

	// Initialize the Bond Manager
	bond_init_data.flash_page_num_bond     = FLASH_PAGE_BOND;
	bond_init_data.flash_page_num_sys_attr = FLASH_PAGE_SYS_ATTR;
	bond_init_data.evt_handler             = ble_bond_manager_event_handler;
	bond_init_data.error_handler           = ble_bond_manager_error_handler;
	bond_init_data.bonds_delete            = delete_bonds;

	err_code = ble_bondmngr_init(&bond_init_data);
	APP_ERROR_CHECK(err_code);
}

