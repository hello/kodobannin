#pragma once
#include "message_base.h"

enum MSG_LEDAddress{
    LED_PLAY_BOOT_COMPLETE = 1,
};

MSG_Base_t * MSG_LEDInit(MSG_Central_t * parent);
void test_led();
