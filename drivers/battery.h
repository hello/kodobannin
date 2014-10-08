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

 /** @cond To make doxygen skip this file */
 
/** @file
 *
 * @defgroup ble_sdk_app_hrs_eval_battery Battery Level Hardware Handling
 * @{
 * @ingroup ble_sdk_app_hrs_eval
 * @brief Battery Level Hardware Handling prototypes
 *
 */

#ifndef BATTERY_H__
#define BATTERY_H__

typedef uint16_t adc_t;
typedef void(*batter_measure_callback_t)(adc_t adc, uint32_t batt_level_milli_volts, uint8_t percentage_battery_level);


/**@brief Function for making the ADC start a battery level conversion.
 */
uint32_t battery_measurement_begin(batter_measure_callback_t callback);
void battery_module_power_off();
void battery_module_power_on();

#endif // BATTERY_H__

/** @} */
/** @endcond */
