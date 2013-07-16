#include "micro-ecc/ecc.h"
#include <app_error.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <nrf_soc.h>
#include <app_timer.h>
#include <device_params.h>
#include <ble_err.h>
#include <ble_bas.h>

#define BATTERY_LEVEL_MEAS_INTERVAL          APP_TIMER_TICKS(2000, APP_TIMER_PRESCALER) /**< Battery level measurement interval (ticks). */

extern void ble_init(void);
extern void ble_advertising_start(void);
extern uint32_t ble_services_update_battery_level(uint8_t level);

static app_timer_id_t m_battery_timer_id; /**< Battery timer. */

/**@brief Perform battery measurement, and update Battery Level characteristic in Battery Service.
 */
static void battery_level_update(void)
{
    uint32_t err_code;
    uint8_t  battery_level;
    
    battery_level = 69;

    err_code = ble_services_update_battery_level(battery_level);
    if (
        (err_code != NRF_SUCCESS)
        &&
        (err_code != NRF_ERROR_INVALID_STATE)
        &&
        (err_code != BLE_ERROR_NO_TX_BUFFERS)
        &&
        (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)
    )
    {
        APP_ERROR_HANDLER(err_code);
    }
}

/**@brief Battery measurement timer timeout handler.
 *
 * @details This function will be called each time the battery level measurement timer expires.
 *
 * @param[in]   p_context   Pointer used for passing some arbitrary information (context) from the
 *                          app_start_timer() call to the timeout handler.
 */
static void battery_level_meas_timeout_handler(void * p_context)
{
    (void)p_context;
    battery_level_update();
}

/**@brief Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void
timers_init(void)
{
    uint32_t err_code;
    
    // Initialize timer module
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, false);

    // Create timers
    err_code = app_timer_create(&m_battery_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                battery_level_meas_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

static void
application_timers_start(void)
{
    uint32_t err_code;

    // Start application timers
    err_code = app_timer_start(m_battery_timer_id, BATTERY_LEVEL_MEAS_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}

void
_start()
{
	uint32_t err_code;
	//APP_ERROR_CHECK(-1);

	timers_init();
	ble_init();

	application_timers_start();
	ble_advertising_start();
	while(1) {
		err_code = sd_app_event_wait();
		APP_ERROR_CHECK(err_code);
		//__WFE();
	}
/*
	nrf_gpio_cfg_output(GPIO_3v3_Enable);
	nrf_gpio_pin_set(GPIO_3v3_Enable);
	nrf_gpio_cfg_output(GPIO_HRS_PWM);

	while (1){
		nrf_gpio_pin_set(GPIO_HRS_PWM);
		nrf_delay_ms(500);
		nrf_gpio_pin_clear(GPIO_HRS_PWM);
		nrf_delay_ms(500);
	}
*/
}
