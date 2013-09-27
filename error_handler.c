#include <ble_types.h>
#include <nrf_delay.h>
#include <nrf51.h>
#include <nrf_gpio.h>
#include "device_params.h"

/**@brief Error handler function, which is called when an error has occurred.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of error.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name.
 */
void
app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *filename)
{
#ifdef ASSERT_LED_PIN_NO
	nrf_gpio_pin_set(ASSERT_LED_PIN_NO);
#else
	nrf_gpio_cfg_output(18);
	while(1) {
	    nrf_gpio_pin_set(18);
	    nrf_delay_ms(500);
	    nrf_gpio_pin_clear(18);
	    nrf_delay_ms(500);
	}
	while(1) { __WFE(); }
#endif
	(void)error_code;
	(void)line_num;
	(void)filename;
	// This call can be used for debug purposes during development of an application.
	// @note CAUTION: Activating this code will write the stack to flash on an error.
	//                This function should NOT be used in a final product.
	//                It is intended STRICTLY for development/debugging purposes.
	//                The flash write will happen EVEN if the radio is active, thus interrupting
	//                any communication.
	//                Use with care. Un-comment the line below to use.
	// ble_debug_assert_handler(error_code, line_num, p_file_name);

	// On assert, the system can only recover on reset
	NVIC_SystemReset();
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

