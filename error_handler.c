#include <ble_types.h>
#include <nrf_delay.h>
#include <nrf51.h>
#include <nrf_gpio.h>

#include "device_params.h"
#include "hello_dfu.h"

/**@brief Error handler function, which is called when an error has occurred.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name.
 */
void
app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *filename)
{
	uint32_t error_led;

	// silence the stupid compiler
	(void)error_code;
	(void)line_num;
	(void)filename;

	// let the bootloader know that we crashed and trap in DFU mode
	NRF_POWER->GPREGRET |= GPREGRET_APP_CRASHED_MASK;
	NRF_POWER->GPREGRET |= GPREGRET_FORCE_DFU_ON_BOOT_MASK;

	// look at the chip's hardware ID to make a guess of which type of device we're
	// running on so that we can blink an LED to alert the user to the crash
	switch ((NRF_FICR->CONFIGID & FICR_CONFIGID_HWID_Msk) >> FICR_CONFIGID_HWID_Pos)
	{
		// for WLCSP packages, which Hello uses in bands
		case 0x31UL:
		case 0x2FUL:
			error_led = GPIO_HRS_PWM_G1;
			nrf_gpio_cfg_output(GPIO_3v3_Enable);
	    		nrf_gpio_pin_set(GPIO_3v3_Enable);
			break;

		// for other chips such as QFN that the Nordic Dev / Eval boards use
		default:
			error_led = 18;
	}

	// blink the error LED at 1Hz forever or until the watchdog reboots us
	nrf_gpio_cfg_output(error_led);

	while(1) {
	    nrf_gpio_pin_set(error_led);
	    nrf_delay_ms(500);
	    nrf_gpio_pin_clear(error_led);
	    nrf_delay_ms(500);
	}
}

/**@brief Assert macro callback function.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void
assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
	app_error_handler(0xDEADBEEF, line_num, p_file_name);
}
