// vi:noet:sw=4 ts=4

#include "ble_event.h"
#include <hlo_ble_demo.h>

extern void ble_conn_params_on_ble_evt(ble_evt_t *);
extern void ble_bond_manager_event_handler(ble_evt_t *);
extern void on_ble_event(ble_evt_t *);

/**@brief Dispatches a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
void
ble_event_dispatch(ble_evt_t *event)
{
	//ble_bps_on_ble_evt(&m_bps, event);
	//ble_bas_on_ble_evt(&m_bas, event);
    ble_conn_params_on_ble_evt(event);
    hlo_ble_on_ble_evt(event);
	hlo_ble_demo_on_ble_evt(event);
	// ble_bond_manager_event_handler(event);
	on_ble_event(event);
}
