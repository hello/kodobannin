#pragma once
#include "message_ant.h"
/**
 * Application specific logic for message_ant module
 * DO NOT block any of the callbacks, dispatch actions via central instead
 */

MSG_ANTHandler_t * ANT_UserInit(MSG_Central_t * central);
void ANT_UserSetPairing(uint8_t enable);

