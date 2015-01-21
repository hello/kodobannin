#pragma once
#include "platform.h"
#include <stdint.h>


void led_init();
void led_power_on(uint8_t mode);
void led_warm_up(uint8_t mode);
void led_power_off(uint8_t mode);

void led_set(int led_channel, int pwmval);
void led_update_battery_status();

//test functions below
void led_all_colors_on();
void led_all_colors_off();

//reed switch
uint32_t led_check_reed_switch(void);
