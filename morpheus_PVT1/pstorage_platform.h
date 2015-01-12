/* Copyright (c)  2013 Nordic Semiconductor. All Rights Reserved.
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

 /** @cond To make doxygen skip this file */

/** @file
 *  This header contains defines with respect persistent storage that are specific to
 *  persistent storage implementation and application use case.
 *  CODE PAGE SIZE in FICR for nrf514 is 0x400
 *  CODE SIZE is 0x100
 *  check memory.ld for number of usable code pages
 */
#ifndef PSTORAGE_PL_H__
#define PSTORAGE_PL_H__

#include <stdint.h>

#define PSTORAGE_FLASH_PAGE_SIZE    ((uint16_t)NRF_FICR->CODEPAGESIZE)   /**< Size of one flash page. */
#define APP_DATA_END_ADDRESS        (0x3D000)                            /** defined in memory.ld */
#define APP_DATA_START_ADDRESS      (0x3F000)                            /** defined in memory.ld*/
#define PSTORAGE_FLASH_EMPTY_MASK    0xFFFFFFFF                          /**< Bit mask that defines an empty address in flash. */

#define PSTORAGE_FLASH_PAGE_END	(APP_DATA_END_ADDRESS/PSTORAGE_FLASH_PAGE_SIZE)	


#define PSTORAGE_MAX_APPLICATIONS   3                                                           /**< Maximum number of applications that can be registered with the module, configurable based on system requirements. */
#define PSTORAGE_MIN_BLOCK_SIZE     0x0010                                                      /**< Minimum size of block that can be registered with the module. Should be configured based on system requirements, recommendation is not have this value to be at least size of word. */

#define PSTORAGE_DATA_START_ADDR    APP_DATA_START_ADDRESS
#define PSTORAGE_DATA_END_ADDR      ((PSTORAGE_FLASH_PAGE_END - 1) * PSTORAGE_FLASH_PAGE_SIZE)  /**< End address for persistent data, configurable according to system requirements. */
#define PSTORAGE_SWAP_ADDR          PSTORAGE_DATA_END_ADDR                                      /**< Top-most page is used as swap area for clear and update. */

#define PSTORAGE_MAX_BLOCK_SIZE     PSTORAGE_FLASH_PAGE_SIZE                                    /**< Maximum size of block that can be registered with the module. Should be configured based on system requirements. And should be greater than or equal to the minimum size. */

#define PSTORAGE_CMD_QUEUE_SIZE     10                                                          /**< Maximum number of flash access commands that can be maintained by the */

#define PSTORAGE_RAW_MODE_ENABLE

#ifdef PSTORAGE_RAW_ACCESS
#ifdef S310_STACK
    #define PSTORAGE_RAW_START_ADDR   0x00020000                                                /**< Start address for Raw access mode, configurable based on use case. */
#else
    #define PSTORAGE_RAW_START_ADDR   0x00014000                                                /**< Start address for Raw access mode, configurable based on use case. */
#endif

#define PSTORAGE_RAW_END_ADDR       0x0003C800                                                  /**< End address for Raw access mode, configurable based on use case. */
#endif // PSTORAGE_RAW_MODE_ENABLE


//#define PSTORAGE_CMD_QUEUE_SIZE     30                                                          /**< Maximum number of flash access commands that can be maintained by the module for all applications. Configurable. */


/** Abstracts persistently memory block identifier. */
typedef uint32_t pstorage_block_t;

typedef struct
{
    uint32_t            module_id;      /**< Module ID.*/
    pstorage_block_t    block_id;       /**< Block ID.*/
} pstorage_handle_t;

typedef uint16_t pstorage_size_t;      /** Size of length and offset fields. */

/**@brief Handles Flash Access Result Events. To be called in the system event dispatcher of the application. */
void pstorage_sys_event_handler (uint32_t sys_evt);

#endif // PSTORAGE_PL_H__

/** @} */
/** @endcond */
