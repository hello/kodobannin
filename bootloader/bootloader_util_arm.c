/* Copyright (c) 2013 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include "bootloader_util.h"
#include <stdint.h>
#include "nordic_common.h"
#include "bootloader_types.h"
#include "dfu_types.h"

//uint8_t  m_boot_settings[CODE_PAGE_SIZE] __attribute__((at(BOOTLOADER_SETTINGS_ADDRESS))) __attribute__((used));          /**< This variable reserves a codepage for bootloader specific settings, to ensure the compiler doesn't locate any code or variables at his location. */
//uint32_t m_uicr_bootloader_start_address __attribute__((at(NRF_UICR_BOOT_START_ADDRESS))) = BOOTLOADER_REGION_START;      /**< This variable ensures that the linker script will write the bootloader start address to the UICR register. This value will be written in the HEX file and thus written to UICR when the bootloader is flashed into the chip. */


void StartApplication(uint32_t start_addr)
{
	asm("LDR   R2, [R0]");
	asm("MSR   MSP, R2");
	asm("LDR   R3, [R0, #0x00000004]");
	asm("BX    R3");
	asm(".align 4");
}

void bootloader_util_app_start(uint32_t start_addr)
{
    StartApplication(start_addr);
}
