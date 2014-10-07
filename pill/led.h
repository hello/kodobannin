#pragma once
typedef void(*led_callback_t)(void);  

void led_power_on();
void led_power_off();
void led_all_colors_on();
void led_all_colors_off();
bool led_flash(uint32_t color_rgb, uint8_t count, led_callback_t on_finished);