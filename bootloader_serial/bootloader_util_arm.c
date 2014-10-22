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


void bootloader_util_settings_get(const bootloader_settings_t ** pp_bootloader_settings)
{
  *pp_bootloader_settings = (const bootloader_settings_t*)BOOTLOADER_SETTINGS_ADDRESS;
}
