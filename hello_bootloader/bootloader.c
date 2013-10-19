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

#include "bootloader.h"
#include "bootloader_types.h"
#include "bootloader_util.h"
#include "dfu.h"
#include "dfu_transport.h"
#include "nrf51.h"
#include "app_error.h"
#include "nrf_sdm.h"
#include "ble_flash.h"
#include "nordic_common.h"

#define IRQ_ENABLED             0x01           /**< Field identifying if an interrupt is enabled. */
#define MAX_NUMBER_INTERRUPTS   32             /**< Maximum number of interrupts available. */


bool bootloader_app_is_valid(uint32_t app_addr)
{
    return false;
}

extern bool dfu_success;

void bootloader_dfu_update_process(dfu_update_status_t update_status)
{
    if (update_status.status_code == DFU_UPDATE_COMPLETE)
    {
        dfu_success = true;
    }
    else if (update_status.status_code == DFU_TIMEOUT)
    {
        // Timeout has occurred. Close the connection with the DFU Controller.
        uint32_t err_code = dfu_transport_close();
        APP_ERROR_CHECK(err_code);
    }
}


uint32_t bootloader_dfu_start(void)
{
    uint32_t err_code;
    
    // Clear swap if banked update is used.    
    err_code = dfu_init(); 
    if (err_code == NRF_SUCCESS)    
    {
        err_code = dfu_transport_update_start();
    }
        
    return err_code;
}


/**@brief Function for disabling all interrupts before jumping from bootloader to application.
 */
void interrupts_disable(void)
{
    uint32_t interrupt_setting_mask;
    uint8_t  irq;

    // We start the loop from first interrupt, i.e. interrupt 0.
    irq                    = 0;
    // Fetch the current interrupt settings.
    interrupt_setting_mask = NVIC->ISER[0];
    
    for (; irq < MAX_NUMBER_INTERRUPTS; irq++)
    {
        if (interrupt_setting_mask & (IRQ_ENABLED << irq))
        {
            // The interrupt was enabled, and hence disable it.
            NVIC_DisableIRQ((IRQn_Type) irq);
        }
    }        
}


void bootloader_app_start(uint32_t app_addr)
{
    // If the applications CRC has been checked and passed, the magic number will be written and we
    // can start the application safely.
    interrupts_disable();

    uint32_t err_code = sd_softdevice_forward_to_application();
    APP_ERROR_CHECK(err_code);

    bootloader_util_app_start(CODE_REGION_1_START);
}
