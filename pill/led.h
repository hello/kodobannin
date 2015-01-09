#pragma once
#include "platform.h"
#include <stdint.h>


void led_init();
void led_power_on();
void led_warm_up();
void led_power_off();
void led_set(int led_channel, int pwmval);
//test functions below
void led_all_colors_on();
void led_all_colors_off();

//reed switch
uint32_t led_check_reedswitch(void);
