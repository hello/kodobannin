// vi:noet:sw=4 ts=4

#include <string.h>

#include <app_error.h>
#include <bootloader_util.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <nrf_sdm.h>
#include <nrf_soc.h>
#include <app_timer.h>
#include <platform.h>
#include <ble_err.h>
#include "sha1.h"
#include <bootloader.h>
#include <dfu_types.h>
#include <bootloader_util_arm.h>
#include <ble_flash.h>
#include <ble_stack_handler_types.h>
#include <softdevice_handler.h>
#include <pstorage.h>

#include <nrf_nvmc.h>

#include "app.h"
#include "platform.h"
#include "error_handler.h"
#include "hello_dfu.h"
#include "util.h"
#include "git_description.h"
#include "factory_provision.h"
#include "gpio_nor.h"


enum {
    APP_GPIOTE_MAX_USERS = 2,
};

 enum {
    SCHED_MAX_EVENT_DATA_SIZE = MAX(APP_TIMER_SCHED_EVT_SIZE, 0),
    SCHED_QUEUE_SIZE = 20,
};

bool dfu_success = false;

extern uint8_t __app_sha1_start__[SHA1_DIGEST_LENGTH];

void
_start()
{
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);

	NRF_CLOCK->TASKS_LFCLKSTART = 1;
	while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0);

#ifdef DEBUG_SERIAL
    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);
    SIMPRINTS("\r\nBootloader v");
	SIMPRINTS(" is alive\r\n");
    SIMPRINTS("Device name: ");
    SIMPRINTS(BLE_DEVICE_NAME);
    SIMPRINTS("\r\n");
#endif

    if((NRF_POWER->GPREGRET & GPREGRET_APP_CRASHED_MASK)) {
#ifdef DEBUG_SERIAL
        SIMPRINTS("Application crashed :(\r\n");
#endif
    }
    bool should_dfu = false;

    const bootloader_settings_t* bootloader_settings;
    bootloader_util_settings_get(&bootloader_settings);

	/*
	 *uint16_t* p_expected_crc = &bootloader_settings->bank_0_crc;
	 *uint16_t expected_crc = *p_expected_crc;
	 *SIMPRINT("CRC-16 is %d\r\n", expected_crc);
	 */

    if(!bootloader_app_is_valid(DFU_BANK_0_REGION_START)) {
#ifdef DEBUG_SERIAL
        SIMPRINTS("Firmware doesn't match expected CRC-16\r\n");
#endif
        should_dfu = true;
	}

    if((NRF_POWER->GPREGRET & GPREGRET_FORCE_DFU_ON_BOOT_MASK)) {
#ifdef DEBUG_SERIAL
        SIMPRINTS("Forcefully booting into DFU mode.\r\n");
#endif
        should_dfu = true;
	}

	if(factory_needs_provisioning()) {
		uint32_t ret;
        SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_RC_250_PPM_250MS_CALIBRATION, true);
        APP_OK(softdevice_sys_evt_handler_set(pstorage_sys_event_handler));
        APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
#ifdef DEBUG_SERIAL
		SIMPRINTS("Provisioning New Key...");
#endif

		ret = factory_provision_start();

		if(ret == NRF_SUCCESS){
#ifdef DEBUG_SERIAL
			SIMPRINTS("Successful\r\n");
#endif
		}else{
#ifdef DEBUG_SERIAL
			SIMPRINTS("Failed\r\n");
#endif
		}
		NVIC_SystemReset();
	}

    if(should_dfu) {
#ifdef DEBUG_SERIAL
	    SIMPRINTS("Bootloader: in DFU mode...\r\n");
#endif

        SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, true);
        APP_OK(softdevice_sys_evt_handler_set(pstorage_sys_event_handler));

        APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);

		APP_OK(bootloader_dfu_start());

		if(bootloader_app_is_valid(DFU_BANK_0_REGION_START)) {
			sd_power_gpregret_clr(GPREGRET_FORCE_DFU_ON_BOOT_MASK);
#ifdef DEBUG_SERIAL
			SIMPRINTS("DFU successful, rebooting...\r\n");
#endif
		}
		NVIC_SystemReset();
    } else {
		gpio_cfg_d0s1_output_disconnect(SERIAL_RX_PIN);
		gpio_cfg_d0s1_output_disconnect(SERIAL_TX_PIN);
		memcpy((uint8_t*)0x20003FF0, decrypt_key(), 0x10);
#ifdef DEBUG_SERIAL
	    SIMPRINTS("Bootloader kicking to app...\r\n");
#endif
		bootloader_app_start(CODE_REGION_1_START);
	}
}
