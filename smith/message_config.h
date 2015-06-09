#pragma once

#include "platform.h"

#ifdef ANT_STACK_SUPPORT_REQD
#include <ant_stack_handler_types.h>
#endif

#define MSG_CENTRAL_MODULE_NUM  (MOD_END)

#ifdef ANT_STACK_SUPPORT_REQD
void ant_handler(ant_evt_t * p_ant_evt);
#define NUM_ANT_CHANNELS 2
#define DEFAULT_ANT_BOND_COUNT 4
#endif


