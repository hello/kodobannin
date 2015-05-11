// vi:noet:sw=4 ts=4

#include <string.h>

#include <app_error.h>
#include <bootloader_util.h>
#include <nrf_gpio.h>
#include <app_gpiote.h>
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
#include <hci_slip.h>
#include <nrf_nvmc.h>

#include "app.h"
#include "platform.h"
#include "error_handler.h"
#include "hello_dfu.h"
#include "util.h"
#include "git_description.h"

enum {
    APP_GPIOTE_MAX_USERS = 2,
};

 enum {
    SCHED_MAX_EVENT_DATA_SIZE = MAX(APP_TIMER_SCHED_EVT_SIZE, 0),
    SCHED_QUEUE_SIZE = 20,
};
bool dfu_success = false;

static uint8_t
_header_checksum_calculate(uint8_t * header){
	uint32_t checksum = header[0];
	checksum += header[1];
	checksum += header[2];
	checksum &= 0xFFu;
	checksum = (~checksum + 1u);
	return (uint8_t)checksum;
}

static void
_slip_prints(char * string){
#ifdef PLATFORM_HAS_SERIAL_CROSS_CONNECT
	char * b = string;
	while(*b){
		app_uart_put(*b);
		b++;
	}
#endif
}
static void
_slip_encode_simple(uint8_t * buffer, uint16_t buffer_size){
	static uint8_t tx_buffer[32];
	if(buffer_size + 4 > sizeof(tx_buffer)){
		_slip_prints("buffer too big kthx\r\n");
		return;
	}
	//sets header
	memset(tx_buffer, 0, 4);
	tx_buffer[1] = (uint8_t)((buffer_size & 0x0F) + 14);
	tx_buffer[2] = (uint8_t)((buffer_size >> 4 ) & 0xFFu);
	tx_buffer[3] = _header_checksum_calculate(tx_buffer);
	//sets body
	memcpy(tx_buffer+4, buffer, buffer_size);
	hci_slip_write(tx_buffer, buffer_size + 4);
}

void
_start()
{
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);
	APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);

	NRF_CLOCK->TASKS_LFCLKSTART = 1;
	while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0);

#ifdef PLATFORM_HAS_SERIAL_CROSS_CONNECT
	APP_OK(hci_slip_open());
#endif
	_slip_prints("BOOTLOADER IS ALIVE\n\r");

	//crash_log_save();

	// const bool firmware_verified = _verify_fw_sha1((uint8_t*)proposed_fw_sha1);

    if((NRF_POWER->GPREGRET & GPREGRET_APP_CRASHED_MASK)) {
        //SIMPRINTS("Application crashed :(\r\n");
    }

    bool should_dfu = false;

    const bootloader_settings_t* bootloader_settings;
    bootloader_util_settings_get(&bootloader_settings);

	/*
	 *uint16_t* p_expected_crc = &bootloader_settings->bank_0_crc;
	 *uint16_t expected_crc = *p_expected_crc;
	 *SIMPRINTS("CRC-16 is %d\r\n", expected_crc);
	 */

    if(!bootloader_app_is_valid(DFU_BANK_0_REGION_START)) {
        //SIMPRINTS("Firmware doesn't match expected CRC-16\r\n");
        should_dfu = true;
	}

    if((NRF_POWER->GPREGRET & GPREGRET_FORCE_DFU_ON_BOOT_MASK)) {
        //SIMPRINTS("Forcefully booting into DFU mode.\r\n");
        should_dfu = true;
	}

    if(should_dfu) {

		_slip_encode_simple("DFUBEGIN",sizeof("DFUBEGIN")+1);

        SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_RC_250_PPM_250MS_CALIBRATION, true);
        APP_OK(softdevice_sys_evt_handler_set(pstorage_sys_event_handler));

        APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);

		APP_OK(bootloader_dfu_start());

		if(bootloader_app_is_valid(DFU_BANK_0_REGION_START)) {
			sd_power_gpregret_clr(GPREGRET_FORCE_DFU_ON_BOOT_MASK);
			//SIMPRINTS("DFU successful, rebooting...\r\n");
		}
		NVIC_SystemReset();
    } else {
	    //SIMPRINTS("Bootloader kicking to app...\r\n");

		bootloader_app_start(CODE_REGION_1_START);
	}
}
