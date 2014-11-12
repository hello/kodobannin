#pragma once
/**
 * led driver
 */

typedef void(*led_callback_t)(void);  

void led_driver_init();
void led_power_on();
void led_warm(void);
void led_power_off();
void led_all_colors_on();
void led_all_colors_off();
bool led_flash(uint8_t color, uint8_t count, led_callback_t on_finished); // unit32_t color_rgb

typedef void(*led_measure_callback_t)(uint16_t adc_reading, uint32_t milli_volts, uint8_t percentage);

