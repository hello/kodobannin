#pragma once
typedef void(*led_callback_t)(void);  

void led_init();
void led_power_on();
void led_warm_up();
void led_power_off();
void led_all_colors_on();
void led_all_colors_off();
bool led_flash(uint8_t color, uint8_t count, led_callback_t on_finished); // unit32_t color_rgb
