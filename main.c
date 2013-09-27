#include "micro-ecc/ecc.h"
#include <app_error.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <nrf_soc.h>
#include <app_timer.h>
#include <device_params.h>
#include <ble_err.h>
#include <ble_bas.h>
#include <simple_uart.h>
#include "sha.h"
#include <bootloader.h>
#include <dfu_types.h>
#include "bootloader_util_arm.h"
#include <ble_flash.h>

extern void ble_init(void);
extern void ble_advertising_start(void);
extern uint32_t ble_services_update_battery_level(uint8_t level);

static app_timer_id_t m_battery_timer_id; /**< Battery timer. */

static ble_bas_t m_bas;

/**@brief Perform battery measurement, and update Battery Level characteristic in Battery Service.
 */
#if 0
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
#endif

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
    // battery_level_update();
    ble_bas_battery_level_update(&m_bas, 69);
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
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);

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

static void
sha1_fw_area(uint8_t *hash)
{
	SHA_CTX ctx;
	uint32_t code_size    = DFU_IMAGE_MAX_SIZE_FULL;
	uint8_t *code_address = (uint8_t *)CODE_REGION_1_START;
	uint32_t *index = (uint32_t *)BOOTLOADER_REGION_START - DFU_APP_DATA_RESERVED - 4;

	// walk back to the end of the actual firmware
	while (*index-- == EMPTY_FLASH_MASK && code_size > 0)
		code_size -= 4;

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, (void *)code_address, code_size);
	SHA1_Final(hash, &ctx);
}

static bool
verify_fw_sha1(uint8_t *valid_hash)
{
	uint8_t sha1[SHA1_DIGEST_LENGTH];
    uint8_t comp = 0;
    int i = 0;

    sha1_fw_area(sha1);

    for (i = 0; i < SHA1_DIGEST_LENGTH; i++)
        comp |= sha1[i] ^ valid_hash[i];

    return comp == 0;
}

#include "ble_stack_handler.h"

#define APP_GPIOTE_MAX_USERS            2
#include "app_gpiote.h"
static void gpiote_init(void)
{
	APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);
}

static void flash_page_erase(uint32_t * p_page)
{
    // Turn on flash erase enable and wait until the NVMC is ready.
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Erase page.
    NRF_NVMC->ERASEPAGE = (uint32_t)p_page;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing.
    }

    // Turn off flash erase enable and wait until the NVMC is ready.
    NRF_NVMC->CONFIG = (NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy)
    {
        // Do nothing
    }
}

void
_start()
{
	uint32_t err_code;

	/*
	nrf_gpio_cfg_output(GPIO_3v3_Enable);
	nrf_gpio_pin_set(GPIO_3v3_Enable);
	*/

	simple_uart_config(0, 9, 0, 10, false);
	//timers_init();
	//ble_init();

	//< hack for when timers are disabled
	ble_bas_battery_level_update(&m_bas, 69);
	//battery_level_update();

	//application_timers_start();

	//ble_advertising_start();

	timers_init();
	gpiote_init();

	//dfu_init();

	volatile uint8_t* proposed_fw_sha1 = (uint8_t*)BOOTLOADER_SETTINGS_ADDRESS;
	while(!verify_fw_sha1(proposed_fw_sha1)) {
		err_code = bootloader_dfu_start();
		APP_ERROR_CHECK(err_code);

		uint8_t new_fw_sha1[SHA1_DIGEST_LENGTH];
		sha1_fw_area(new_fw_sha1);

		flash_page_erase((uint32_t*)BOOTLOADER_SETTINGS_ADDRESS);
		ble_flash_block_write((uint32_t*)BOOTLOADER_SETTINGS_ADDRESS, (uint32_t*)new_fw_sha1, 5); // SHA1_DIGEST_LENGTH/sizeof(word)
	}

	interrupts_disable();

	err_code = sd_softdevice_forward_to_application();
	APP_ERROR_CHECK(err_code);

	StartApplication(CODE_REGION_1_START);
}
