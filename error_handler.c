// vi:sw=4 ts=4

#include <string.h>
#include <unwind.h>

#include <ble_types.h>
#include <nrf_delay.h>
#include <nrf51.h>
#include <nrf_gpio.h>
#include <nrf_nvmc.h>
#include <nrf_sdm.h>

#include "device_params.h"
#include "error_handler.h"
#include "hello_dfu.h"
#include "util.h"

enum crash_log_signature {
    CRASH_LOG_SIGNATURE_APP_ERROR = 0xA5BE5705,
	CRASH_LOG_SIGNATURE_HARDFAULT = 0xDEADBEA7,
};

static void* const CRASH_LOG_ADDRESS = (uint8_t*) 0x20000004;

static void
_save_stack(uint8_t* stack_start, struct crash_log* crash_log)
{
    const uint8_t* RAM_END = (uint8_t*) 0x20004000;
    crash_log->stack_size = RAM_END - stack_start;
    memcpy(&crash_log->stack[0], stack_start, crash_log->stack_size);
}

void
app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *filename)
{
	printf("[ERROR] app_error_handler (%s:%d) error_code=%d\r\n", filename, line_num, error_code);

	(void) sd_softdevice_disable();

	// let the bootloader know that we crashed
	NRF_POWER->GPREGRET |= GPREGRET_APP_CRASHED_MASK;

	struct crash_log* crash_log = CRASH_LOG_ADDRESS;

    crash_log->signature = CRASH_LOG_SIGNATURE_APP_ERROR;
	size_t filename_len = MAX(strlen((char* const)filename), sizeof(crash_log->app_error.filename));
	memcpy(crash_log->app_error.filename, filename, filename_len);
	crash_log->app_error.line_number = line_num;
	crash_log->app_error.error_code = error_code;

    uint8_t* stack_start = (uint8_t*)&crash_log;
    _save_stack(stack_start, crash_log);

#define KODOBANNIN_APP_ERROR_BKPT

#ifdef KODOBANNIN_APP_ERROR_BKPT
    __asm("bkpt #0\n"); // Break into the debugger, or reboot if no debugger attached.
#else
    uint32_t error_led;

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
#endif
}

void
assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
	app_error_handler(0xDEADBEEF, line_num, p_file_name);
}

void
crash_log_save()
{
    struct crash_log* crash_log = CRASH_LOG_ADDRESS;

	switch(crash_log->signature) {
	case CRASH_LOG_SIGNATURE_HARDFAULT:
		DEBUG("Found HardFault crash log at ", CRASH_LOG_ADDRESS);
		break;
	case CRASH_LOG_SIGNATURE_APP_ERROR:
        DEBUG("Found app error crash log at ", CRASH_LOG_ADDRESS);
		break;
	default:
		DEBUG("No crash log found, signature is: ", crash_log->signature);
		return;
	}

	uint32_t size = sizeof(struct crash_log) + crash_log->stack_size;

	DEBUG("Crash log size: ", size);

	// Save to external Flash here

	PRINTS("Crash log saved.\r\n");

	memset(crash_log, 0, size);
}

void
band_hardfault_handler(unsigned long stacked_registers[8])
{
	NRF_POWER->GPREGRET |= GPREGRET_APP_CRASHED_MASK;

	struct crash_log* crash_log = CRASH_LOG_ADDRESS;

	crash_log->signature = CRASH_LOG_SIGNATURE_HARDFAULT;

	memcpy(crash_log->hardfault.stacked_registers, stacked_registers, sizeof(stacked_registers));

    uint8_t* stack_start = (uint8_t*)stacked_registers + sizeof(stacked_registers);
	_save_stack(stack_start, crash_log);

	__asm("bkpt #0\n"); // Break into the debugger, or reboot if no debugger attached.

#if 0
	// Configurable Fault Status Register
	// Consists of MMSR, BFSR and UFSR
	volatile unsigned long _CFSR = (*((volatile unsigned long *)(0xE000ED28))) ;
	// Hard Fault Status Register
	volatile unsigned long _HFSR = (*((volatile unsigned long *)(0xE000ED2C))) ;
	// Debug Fault Status Register
	volatile unsigned long _DFSR = (*((volatile unsigned long *)(0xE000ED30))) ;
	// Auxiliary Fault Status Register
	volatile unsigned long _AFSR = (*((volatile unsigned long *)(0xE000ED3C))) ;
	// Read the Fault Address Registers. These may not contain valid values.
	// Check BFARVALID/MMARVALID to see if they are valid values
	// MemManage Fault Address Register
	volatile unsigned long _MMAR = (*((volatile unsigned long *)(0xE000ED34))) ;
	// Bus Fault Address Register
	volatile unsigned long _BFAR = (*((volatile unsigned long *)(0xE000ED38))) ;
#endif
}

void HardFault_Handler() __attribute__((naked));
void HardFault_Handler()
{
	__asm volatile (
					// " movs r0,#4       \n"
					// " movs r1, lr      \n"

					" mrs r0, msp \n"
					// " ldr r1, [r0,#20] \n"

					// Replace the stacked LR, which has been corrupted by SoftDevice, with the stacked LR
					// " ldr r2, [r0,#24] \n"
					// " mov lr, r2 \n"

					" b band_hardfault_handler \n"
					);
}
