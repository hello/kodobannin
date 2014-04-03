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
 
/**@file
 *
 * @defgroup nrf_bootloader_types Types and definitions.
 * @{     
 *  
 * @ingroup nrf_bootloader
 * 
 * @brief Bootloader module type and definitions.
 */
 
#ifndef BOOTLOADER_TYPES_H__
#define BOOTLOADER_TYPES_H__

#include <stdint.h>

/**@brief DFU Bank state code, which indicates wether the bank contains: A valid image, invalid image, or an erased flash.
  */
typedef enum
{
    BANK_VALID_APP   = 0x01,
    BANK_ERASED      = 0xFE, 
    BANK_INVALID_APP = 0xFF,
} bootloader_bank_code_t;

#endif // BOOTLOADER_TYPES_H__ 

/**@} */
