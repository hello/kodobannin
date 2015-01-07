#pragma once
#include "message_base.h"

enum MSG_LEDAddress{
    LED_PLAY_BOOT_COMPLETE = 1,
    LED_PLAY_SHIP_MODE = 2,
    LED_PLAY_BATTERY_TEST = 3,
    LED_PLAY_LED_RGB_TEST = 4,
};

MSG_Base_t * MSG_LEDInit(MSG_Central_t * parent);
void test_led();
void test_bat();
void test_rgb();
