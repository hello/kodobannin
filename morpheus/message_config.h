#pragma once
/*
 * Here define the modules used
 */
#include "platform.h"

#define MSG_BASE_SHARED_POOL_SIZE 12
#define MSG_BASE_DATA_BUFFER_SIZE 24

#ifdef MSG_BASE_USE_BIG_POOL
#define MSG_BASE_SHARED_POOL_SIZE_BIG 6
#define MSG_BASE_DATA_BUFFER_SIZE_BIG 256
#endif

typedef enum{
    CENTRAL = 0,
    UART,
    BLE,
    ANT,
    RTC,
    CLI,
    SSPI,
    TIME,
    MOD_END
}MSG_ModuleType;

#ifdef ANT_ENABLE
#include <ant_stack_handler_types.h>
void ant_handler(ant_evt_t * p_ant_evt);
#endif

#define MSG_CENTRAL_MODULE_NUM  (MOD_END)
