// vi:sw=4:ts=4

#include <app_error.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <nrf_soc.h>
#include <app_timer.h>
#include <device_params.h>
#include <ble_err.h>
#include "sha1.h"
#include <bootloader.h>
#include <dfu_types.h>
#include <bootloader_util_arm.h>
#include <ble_flash.h>
#include <ble_stack_handler.h>
#include <string.h>
#include "hello_dfu.h"
#include "util.h"

#define APP_GPIOTE_MAX_USERS            2

static void
_sha1_fw_area(uint8_t *hash)
{
	uint32_t err;

	uint32_t code_size    = DFU_IMAGE_MAX_SIZE_FULL;
	uint8_t *code_address = (uint8_t *)CODE_REGION_1_START;
	uint32_t *index = (uint32_t *)BOOTLOADER_REGION_START - DFU_APP_DATA_RESERVED;

	// walk back to the end of the actual firmware
	while (*--index == EMPTY_FLASH_MASK && code_size > 0)
		code_size -= 4;

    // only measure if there is something to measure
    memset(hash, 0, SHA1_DIGEST_LENGTH);
    if (code_size ==  0)
        return;

	NRF_RTC1->TASKS_START = 1;

	uint32_t start_ticks, stop_ticks, diff_ticks;
	start_ticks = NRF_RTC1->COUNTER;

    sha1_calc(code_address, code_size, hash);

	stop_ticks = NRF_RTC1->COUNTER;

	debug_print_ticks("SHA1 time in ticks: ", start_ticks, stop_ticks);

	NRF_RTC1->TASKS_STOP = 1;
}

static bool
_verify_fw_sha1(uint8_t *valid_hash)
{
	uint8_t sha1[SHA1_DIGEST_LENGTH];
    uint8_t comp = 0;
    int i = 0;

    _sha1_fw_area(sha1);

#ifdef DEBUG
    DEBUG("SHA1: ", sha1);
#endif

    for (i = 0; i < SHA1_DIGEST_LENGTH; i++)
        comp |= sha1[i] ^ valid_hash[i];

    return comp == 0;
}

static void
_flash_page_erase(uint32_t * p_page)
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

bool dfu_success = false;

void
_start()
{
	uint32_t err_code;
    volatile uint8_t* proposed_fw_sha1 = (uint8_t*)BOOTLOADER_SETTINGS_ADDRESS;
    uint8_t new_fw_sha1[SHA1_DIGEST_LENGTH];

    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);

	NRF_CLOCK->TASKS_LFCLKSTART = 1;
	while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0);

    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);

    PRINTS("Bootloader is alive.\r\n");

#ifdef DEBUG
    PRINTS("Device name: ");
    PRINTS(BLE_DEVICE_NAME);
    PRINTS("\r\n");

	{
		uint8_t mac_address[6];

		// MAC address is stored backwards; reverse it.
		unsigned i;
		for(i = 0; i < 6; i++) {
			mac_address[i] = ((uint8_t*)NRF_FICR->DEVICEADDR)[5-i];
		}
		DEBUG("MAC address: ", mac_address);
	}
#endif

	const bool firmware_verified = _verify_fw_sha1((uint8_t*)proposed_fw_sha1);
    if((NRF_POWER->GPREGRET & GPREGRET_FORCE_DFU_ON_BOOT_MASK) || !firmware_verified) {
	    PRINTS("Bootloader: in DFU mode...\r\n");
		err_code = bootloader_dfu_start();
		APP_ERROR_CHECK(err_code);

		if (dfu_success) {
			NRF_POWER->GPREGRET &= ~GPREGRET_FORCE_DFU_ON_BOOT_MASK;

			// Need to turn the radio off before calling ble_flash_block_write?
			ble_flash_on_radio_active_evt(false);

			_sha1_fw_area(new_fw_sha1);
			_flash_page_erase((uint32_t*)BOOTLOADER_SETTINGS_ADDRESS);
			ble_flash_block_write((uint32_t*)BOOTLOADER_SETTINGS_ADDRESS, (uint32_t*)new_fw_sha1, SHA1_DIGEST_LENGTH/sizeof(uint32_t));
		}

		NVIC_SystemReset();
    } else {
	    PRINTS("Bootloader kicking to app\r\n");
		interrupts_disable();

		err_code = sd_softdevice_forward_to_application();
		APP_ERROR_CHECK(err_code);

		StartApplication(CODE_REGION_1_START);
	}
}
