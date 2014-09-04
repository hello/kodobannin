#pragma once
#include "message_ant.h"
/**
 * Application specific logic for message_ant module
 * DO NOT block any of the callbacks, dispatch actions via central instead
 */

MSG_ANTHandler_t * morpheus_ant_user(MSG_Central_t * central);
