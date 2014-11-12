#pragma once
#include "platform.h"
typedef void(*led_callback_t)(void);  

#define LED_BLUE_CHANNEL LED1_ENABLE
#define LED_RED_CHANNEL LED3_ENABLE
#define LED_GREEN_CHANNEL LED2_ENABLE

void led_init();
void led_power_on();
void led_warm_up();
void led_power_off();
void led_set(int led_channel, int pwmval);
//test functions below
void led_all_colors_on();
void led_all_colors_off();
bool led_flash(uint8_t color, uint8_t count, led_callback_t on_finished); // unit32_t color_rgb
