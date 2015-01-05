/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
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
 *
 * @defgroup ble_sdk_app_hrs_eval_main main.c
 * @{
 * @ingroup ble_sdk_app_hrs_eval
 * @brief Main file for Heart Rate Service Sample Application for nRF51822 evaluation board
 *
 * This file contains the source code for a sample application using the Heart Rate service
 * (and also Battery and Device Information services) for the nRF51822 evaluation board (PCA10001).
 * This application uses the @ref ble_sdk_lib_conn_params module.
 */

#include <stdint.h>
#include <string.h>

#include <util.h>

#include "platform.h"
#include "battery_config.h"
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "nrf_gpio.h"
#include "nrf51_bitfields.h"
#include "softdevice_handler.h"
#include "battery.h"
#include "gpio_nor.h"

//         Vbat          80% 3.0 2.8 20%
//         Vrgb     4.2  3.6    2.2 1.6       0.0
//  6.30  6.00  5.00  4.00  3.00  2.00  1.00  0.00 -1.00 -2.00
//  1.80  1.64  1.45  1.16  0.87  0.58  0.29  0.00 -0.29 -0.58
//  03FE  03D0  355A  02E4  026F  01F9  0186  0108  0080  0006
//  1.20  1.15  1.00  0.85  0.70  0.55  0.40  0.25  0.10  0.05

// adc 0x02E4  ( 740 x 3950 x 1200 ) / 1023  ==>  3.429  Volts
// adc 0x02E4  ( 740 x 3430 x 1200 ) / 1023  ==>  3.064  Volts
//     0x01DC  ( 476 x 7125 x 1200 ) / 1023  ==>  3.98   Volts  <=== correct

// adc 0x026F  ( 623 x 3950 x 1200 ) / 1023  ==>  2.886  Volts
// adc 0x026F  ( 623 x 3430 x 1200 ) / 1023  ==>  2.506  Volts
//     0x0167  ( 359 x 7125 x 1200 ) / 1023  ==>  3.00   Volts  <=== correct

// adc 0x0235  ( 565 x 3950 x 1200 ) / 1023  ==>  2.617  Volts
// adc 0x0235  ( 565 x 3430 x 1200 ) / 1023  ==>  2.273  Volts
//     0x012C  ( 300 x 7125 x 1200 ) / 1023  ==>  2.50   Volts  <=== correct

// adc 0x01F9  ( 505 x 3950 x 1200 ) / 1023  ==>  2.339  Volts
// adc 0x01F9  ( 505 x 3430 x 1200 ) / 1023  ==>  2.091  Volts
//     0x00F1  ( 241 x 7125 x 1200 ) / 1023  ==>  2.01   Volts  <=== correct

/**@brief Macro to convert the result of ADC conversion in millivolts.
 *
 * @param[in]  ADC_VALUE   ADC result.
 * @retval     Result converted to millivolts.
 */

static adc_t _adc_config_offset;
//static adc_t _adc_config_reading;

//#define ADC_BATTERY_IN_MILLI_VOLTS(ADC_VALUE)   ((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS / 1023 * ADC_PRE_SCALING_COMPENSATION)
#define ADC_BATTERY_IN_MILLI_VOLTS(ADC_VALUE)  (((((ADC_VALUE - 0x010A) * 1200 ) / 1023 ) * 7125 ) / 1000 )
//#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE)     ((((ADC_REF_VOLTAGE_IN_MILLIVOLTS)))
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE)  (((ADC_VALUE) * 1200 ) / 1023 )

static uint8_t _adc_config_psel;

inline void battery_module_power_off()
{
    if(ADC_ENABLE_ENABLE_Disabled != NRF_ADC->ENABLE)
    {
        NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Disabled;
    }
#ifdef PLATFORM_HAS_VERSION
    nrf_gpio_pin_set(VBAT_VER_EN); // negate to deactive Vbat resistor divider

    _adc_config_psel = 0; // release clearing adc port select
#endif
}

static uint16_t _battery_level_voltage; // measured battery voltage
static uint8_t _battery_level_percent; // computed remaining capacity

static inline uint8_t _battery_level_in_percent(const uint16_t milli_volts)
{
    _battery_level_voltage = milli_volts;

    if (_battery_level_voltage >= 2950)
    {
        _battery_level_percent = 100;
    }
    else if (_battery_level_voltage > 2900)
    {
        _battery_level_percent = 80;
    }
    else if (_battery_level_voltage > 2850)
    {
        _battery_level_percent = 60;
    }
    else if (_battery_level_voltage > 2800)
    {
        _battery_level_percent = 40;
    }
    else if (_battery_level_voltage > 2750)
    {
        _battery_level_percent = 20;
    }
    else
    {
        _battery_level_percent = 5;
    }

    return _battery_level_percent;
}

uint16_t battery_set_offset_cached(adc_t adc_result) // need to average offset
{
 // if (_adc_config_psel == LDO_VBAT_ADC_INPUT) { // battery being measured
     // _adc_config_offset = adc_result;
     // battery_set_voltage_cached(_adc_config_reading);
 // }
    return _battery_level_voltage;
}

uint16_t battery_set_voltage_cached(adc_t adc_result)
{
 // if (_adc_config_psel == LDO_VBAT_ADC_INPUT) { // battery being measured
     // _adc_config_reading = adc_result;
        _battery_level_voltage = ADC_BATTERY_IN_MILLI_VOLTS((uint32_t) adc_result);
        _battery_level_in_percent(_battery_level_voltage);
 // }
    return _battery_level_voltage;
}

static adc_measure_callback_t _adc_measure_callback;

static uint8_t _adc_cycle_count;
static uint8_t _adc_config_count;

/* @brief Function for handling the ADC interrupt.
 * @details  This function will fetch the conversion result from the ADC, convert the value into
 *           percentage and send it to peer.
 */

void ADC_IRQHandler(void)
{
    uint8_t next_measure_input = 0; // indicate adc sequence complete

    if (NRF_ADC->EVENTS_END)
    {
        NRF_ADC->EVENTS_END     = 0;
        adc_t adc_result      = NRF_ADC->RESULT;
        NRF_ADC->TASKS_STOP     = 1;
        uint16_t adc_count    = ++_adc_cycle_count;

        if(_adc_measure_callback && _adc_config_count--) // callback provided
        {
            next_measure_input = _adc_measure_callback(adc_result, adc_count);
         // if (next_measure_input = 0xFF) next_measure_input = _adc_config_psel;

            if (next_measure_input) // continue adc measurement
            {
                _adc_config_psel = next_measure_input; // save adc input selection

                NRF_ADC->CONFIG = (ADC_CONFIG_RES_10bit << ADC_CONFIG_RES_Pos) |
                                  (ADC_CONFIG_INPSEL_AnalogInputNoPrescaling << ADC_CONFIG_INPSEL_Pos) |
                                  (ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos)  |
                /* port select */ (next_measure_input << ADC_CONFIG_PSEL_Pos)  |
                                  (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);

                NRF_ADC->TASKS_STOP = 0; // Not sure if this is required.
                // Trigger a new ADC sampling, the callback will be called again
                NRF_ADC->TASKS_START = 1; // start adc to make next reading

            } // else { // adc measurement sequence complete
        } // else { // no callback provided
    }

    if (next_measure_input == 0) battery_module_power_off();
}

uint16_t battery_get_voltage_cached(){
#ifdef PLATFORM_HAS_VERSION
    return _battery_level_voltage;
#else
    return BATTERY_INVALID_MEASUREMENT;
#endif
}

uint8_t battery_get_percent_cached(){
#ifdef PLATFORM_HAS_VERSION
    return _battery_level_percent;
#else
    return BATTERY_INVALID_MEASUREMENT;
#endif
}

adc_t battery_get_offset_cached(){
#ifdef PLATFORM_HAS_VERSION
    return _adc_config_offset;
#else
    return BATTERY_INVALID_MEASUREMENT;
#endif
}

void battery_init()
{
    _adc_config_psel = 0; // indicate adc released

    nrf_gpio_pin_set(VBAT_VER_EN); // negate open drain pin inactive to
    nrf_gpio_cfg_output(VBAT_VER_EN); // disable Vbat resistor divider

 // power_on gpio_config
 // gpio_input_disconnect(VMCU_SENSE);
 // gpio_cfg_s0s1_output_connect(VBAT_VER_EN, 0);
 // nrf_gpio_cfg_input(VBAT_SENSE, NRF_GPIO_PIN_NOPULL);

 // power_off gpio_config
 // gpio_cfg_s0s1_output_connect(VBAT_VER_EN, 1);
 // gpio_cfg_d0s1_output_disconnect(VBAT_VER_EN);  // on: 0
 // gpio_input_disconnect(VMCU_SENSE);
 // gpio_input_disconnect(VBAT_SENSE);
}

void battery_module_power_on()
{
#ifdef PLATFORM_HAS_VERSION
 // if (_adc_config_psel) {
 //     return 0; // adc busy with prior reading sequence
 // } else {
        _adc_config_psel = LDO_VBAT_ADC_INPUT;

     // _battery_initial_voltage = 0; // indicate start of measurement sequence
     // _battery_difference_voltage = 0; // indicate no droop during measurement

        nrf_gpio_pin_clear(VBAT_VER_EN); // assert to active Vbat resistor divider
 // }
#endif
}

uint32_t battery_measurement_begin(adc_measure_callback_t callback)
{
#ifdef PLATFORM_HAS_VERSION
    uint32_t err_code;

    _adc_measure_callback = callback; // returning next adc input (result, count)

    _adc_cycle_count = 0; // indicate number of adc cycles so far
    _adc_config_count = 64; // indicate max number of adc cycles

    // Configure ADC
    NRF_ADC->INTENSET   = ADC_INTENSET_END_Msk;
    NRF_ADC->CONFIG     = (ADC_CONFIG_RES_10bit << ADC_CONFIG_RES_Pos)     |
                        (ADC_CONFIG_INPSEL_AnalogInputNoPrescaling << ADC_CONFIG_INPSEL_Pos)  |
                        (ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos)  |
     /* Vbat Sense */  (_adc_config_psel << ADC_CONFIG_PSEL_Pos)    |
                        (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos);
  // NRF_ADC->INTENSET   = ADC_INTENSET_END_Msk;
    NRF_ADC->EVENTS_END = 0;
    NRF_ADC->ENABLE     = ADC_ENABLE_ENABLE_Enabled;

    // Enable ADC interrupt
    
    err_code = sd_nvic_ClearPendingIRQ(ADC_IRQn);
    APP_ERROR_CHECK(err_code);

    err_code = sd_nvic_SetPriority(ADC_IRQn, NRF_APP_PRIORITY_LOW);
    APP_ERROR_CHECK(err_code);

    err_code = sd_nvic_EnableIRQ(ADC_IRQn);
    APP_ERROR_CHECK(err_code);
    
    NRF_ADC->EVENTS_END  = 0;    // Stop any running conversions.
    NRF_ADC->TASKS_START = 1;

    return err_code;
#else
    return NRF_SUCCESS;
#endif
}

/**
 * @}
 */
