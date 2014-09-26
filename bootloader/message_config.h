#pragma once

#include "platform.h"

#ifdef ANT_STACK_SUPPORT_REQD
#include <ant_stack_handler_types.h>
#endif

/*
 * Here define the modules used
 */

#define MSG_BASE_SHARED_POOL_SIZE 16
#define MSG_BASE_DATA_BUFFER_SIZE (8 * sizeof(uint32_t))

#ifdef MSG_BASE_USE_BIG_POOL
#define MSG_BASE_SHARED_POOL_SIZE_BIG 6
#define MSG_BASE_DATA_BUFFER_SIZE_BIG 256
#endif

#define MSG_CENTRAL_MODULE_NUM  10

#ifdef ANT_STACK_SUPPORT_REQD
void ant_handler(ant_evt_t * p_ant_evt);

#define NUM_ANT_CHANNELS 2
#endif

