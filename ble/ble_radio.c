#include <app_error.h>
#include <ble_flash.h>
#include <ble_radio_notification.h>
#include <ble_types.h>
#include <nrf_soc.h>

/**@brief Initialize Radio Notification event handler.
 */
void
radio_notification_init(void)
{
	uint32_t err_code;

	err_code = ble_radio_notification_init(NRF_APP_PRIORITY_HIGH,
	                                       NRF_RADIO_NOTIFICATION_DISTANCE_4560US,
	                                       ble_flash_on_radio_active_evt);
	APP_ERROR_CHECK(err_code);
}

