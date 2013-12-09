/* Copyright (c) 2009 Nordic Semiconductor. All Rights Reserved.
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

/** @file
* @brief Example template project.
* @defgroup nrf_templates_example Example template
* @{
* @ingroup nrf_examples_nrf6310
*
* @brief Example template.
*
*/

#include <stdbool.h>
#include <stdint.h>
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_pwm.h"

int main(void)
{
    // Initialize the PWM library
    // PS! To change the number of PWM channels, please modify PWM_NUM_CHANNELS in nrf_pwm.h
    nrf_pwm_init(8, 9, 10, PWM_MODE_LED_255);

    // Configure the ADC
    // P0.1 is used for ADC input, apply a varying voltage between 0 and VDD to change the PWM values
    NRF_ADC->CONFIG = ADC_CONFIG_RES_8bit << ADC_CONFIG_RES_Pos | ADC_CONFIG_INPSEL_AnalogInputOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos |
                          ADC_CONFIG_REFSEL_SupplyOneThirdPrescaling << ADC_CONFIG_REFSEL_Pos | ADC_CONFIG_PSEL_AnalogInput2 << ADC_CONFIG_PSEL_Pos;
    NRF_ADC->ENABLE = 1;  
    NRF_ADC->INTENSET = ADC_INTENSET_END_Msk;
    
    // Enable the ADC interrupt, and set the priority to 1
    NVIC_SetPriority(ADC_IRQn, 1);
    NVIC_EnableIRQ(ADC_IRQn);   

    // Start the ADC
    NRF_ADC->TASKS_START = 1;
    
    while (true)
    {
    }
}

void ADC_IRQHandler(void)
{
    // Clear the END event
    NRF_ADC->EVENTS_END = 0;
    
    // Read the ADC result, and update the PWM channels accordingly
    uint8_t tmp = NRF_ADC->RESULT;
    nrf_pwm_set_value(0, tmp);
    nrf_pwm_set_value(1, 255 - tmp);
    nrf_pwm_set_value(2, 0);
    
    // Trigger a new ADC sampling
    NRF_ADC->TASKS_START = 1;
}


