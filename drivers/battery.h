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
typedef uint8_t(*adc_measure_callback_t)(adc_t adc_result, uint16_t adc_count); // return next adc input port select
#define BATTERY_INVALID_MEASUREMENT 0xFF
#define BATTERY_EXCEPTION_BASE  0xC8

/**@brief Function for making the ADC start a battery level conversion.
 */
uint32_t battery_measurement_begin(adc_measure_callback_t callback, uint16_t count);
uint8_t battery_level_measured(adc_t result, uint16_t count);

void battery_update_level(); // issue  heartbeat packet
void battery_update_droop(); // else monitor voltage droop

void battery_init();
void battery_module_power_on();
void battery_module_power_off();

void battery_set_result_cached(adc_t result);
void battery_set_offset_cached(adc_t result);
uint16_t battery_set_voltage_cached(adc_t result);

adc_t battery_get_offset_cached();
uint8_t battery_get_percent_cached();
uint16_t battery_get_initial_cached(uint8_t mode);
uint16_t battery_get_voltage_cached();

#endif // BATTERY_H__

/** @} */
/** @endcond */
