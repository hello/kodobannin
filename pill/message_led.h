#pragma once
#include "message_base.h"

enum MSG_LEDAddress{
    LED_PLAY_BOOT_COMPLETE = 1,
    LED_PLAY_SHIP_MODE,
    LED_PLAY_BATTERY_TEST,
#ifdef PLATFORM_HAS_VLED
    LED_PLAY_LED_RED_TEST,
    LED_PLAY_LED_GRN_TEST,
    LED_PLAY_LED_BLU_TEST,
    LED_PLAY_LED_RGB_TEST,
#endif
    LED_PLAY_TEST,
    LED_PLAY_ON,
    LED_COMMAND_SIZE,//reserved, add commands above
};

MSG_Base_t * MSG_LEDInit(MSG_Central_t * parent);

void test_bat();
void test_red();
void test_grn();
void test_blu();
void test_rgb();
void test_led();
