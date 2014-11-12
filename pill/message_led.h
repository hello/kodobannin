#pragma once
#include "message_base.h"

enum MSG_LEDAddress{
    LED_BLINK_GREEN = 1,
};

MSG_Base_t * MSG_LEDInit(MSG_Central_t * parent);
void test_led();
