#pragma once
/*
 * Here define the modules used
 */
#include "platform.h"

#define MSG_BASE_SHARED_POOL_SIZE 11
#define MSG_BASE_DATA_BUFFER_SIZE 32

#ifdef MSG_BASE_USE_BIG_POOL
#define MSG_BASE_SHARED_POOL_SIZE_BIG 4
#define MSG_BASE_DATA_BUFFER_SIZE_BIG 156
#endif

#ifdef ANT_STACK_SUPPORT_REQD
#include <ant_stack_handler_types.h>
void ant_handler(ant_evt_t * p_ant_evt);
#define NUM_ANT_CHANNELS 8
#define DEFAULT_ANT_BOND_COUNT 4
#endif

#define MSG_CENTRAL_MODULE_NUM  (MOD_END)
