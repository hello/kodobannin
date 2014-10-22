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

#include <nrf_nvmc.h>

#include "app.h"
#include "platform.h"
#include "error_handler.h"
#include "hello_dfu.h"
#include "util.h"
#ifdef ECC_BENCHMARK
#include "ecc_benchmark.h"
#endif
#include "git_description.h"

enum {
    APP_GPIOTE_MAX_USERS = 2,
};

 enum {
    SCHED_MAX_EVENT_DATA_SIZE = MAX(APP_TIMER_SCHED_EVT_SIZE, 0),
    SCHED_QUEUE_SIZE = 20,
};

static void
_sha1_fw_area(uint8_t *hash)
{

	uint32_t code_size    = DFU_IMAGE_MAX_SIZE_FULL;
	uint8_t *code_address = (uint8_t *)CODE_REGION_1_START;
	uint32_t *index = (uint32_t *)BOOTLOADER_REGION_START - DFU_APP_DATA_RESERVED;

	// walk back to the end of the actual firmware
	while (index > CODE_REGION_1_START && *--index == EMPTY_FLASH_MASK && code_size > 0)
		code_size -= 4;

    // only measure if there is something to measure
    memset(hash, 0, SHA1_DIGEST_LENGTH);
    if (code_size ==  0)
        return;

	NRF_RTC1->TASKS_START = 1;

	uint32_t start_ticks, stop_ticks;

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

bool dfu_success = false;

extern uint8_t __app_sha1_start__[SHA1_DIGEST_LENGTH];

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
	uint32_t err_code;
    volatile uint8_t* proposed_fw_sha1 = __app_sha1_start__;

    uint8_t new_fw_sha1[SHA1_DIGEST_LENGTH];

    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);
	APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);

	NRF_CLOCK->TASKS_LFCLKSTART = 1;
	while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0);

#ifdef PLATFORM_HAS_SERIAL_CROSS_CONNECT
	APP_OK(hci_slip_open());
#endif
	_slip_prints("BOOTLOADER IS ALIVE\n\r");
	_slip_encode_simple("HelloWorld",sizeof("HelloWorld"));

	crash_log_save();

#ifdef DEBUG
    //SIMPRINTS("Device name: ");
    //SIMPRINTS(BLE_DEVICE_NAME);
    //SIMPRINTS("\r\n");

	{
		uint8_t mac_address[6];

		// MAC address is stored backwards; reverse it.
		unsigned i;
		for(i = 0; i < 6; i++) {
			mac_address[i] = ((uint8_t*)NRF_FICR->DEVICEADDR)[5-i];
		}
		/*
		 *DEBUG("MAC address: ", mac_address);
		 */
	}
#endif

#ifdef ECC_BENCHMARK
	ecc_benchmark();
#endif

	// const bool firmware_verified = _verify_fw_sha1((uint8_t*)proposed_fw_sha1);

    if((NRF_POWER->GPREGRET & GPREGRET_APP_CRASHED_MASK)) {
        //SIMPRINTS("Application crashed :(\r\n");
    }

    bool should_dfu = false;

    const bootloader_settings_t* bootloader_settings;
    bootloader_util_settings_get(&bootloader_settings);

    uint16_t* p_expected_crc = &bootloader_settings->bank_0_crc;

    uint16_t* p_bank_0 = &bootloader_settings->bank_0;

    uint16_t bank_0 = *p_bank_0;

    uint32_t *p_bank_0_size = &bootloader_settings->bank_0_size;

    uint32_t bank_0_size = *p_bank_0_size;

    uint16_t expected_crc = *p_expected_crc;
    ////SIMPRINTS("CRC-16 is %d\r\n", expected_crc);

    if(!bootloader_app_is_valid(DFU_BANK_0_REGION_START)) {
        //SIMPRINTS("Firmware doesn't match expected CRC-16\r\n");
        should_dfu = true;
	}

    if((NRF_POWER->GPREGRET & GPREGRET_FORCE_DFU_ON_BOOT_MASK)) {
        //SIMPRINTS("Forcefully booting into DFU mode.\r\n");
        should_dfu = true;
	}

    if(should_dfu) {
	    //SIMPRINTS("Bootloader: in DFU mode...\r\n");

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
