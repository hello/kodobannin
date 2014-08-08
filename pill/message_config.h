#pragma once
/*
 * Here define the modules used
 */

#define ANT_ENABLE

#define MSG_BASE_SHARED_POOL_SIZE 12
#define MSG_BASE_DATA_BUFFER_SIZE (8 * sizeof(uint32_t))

#define MSG_CENTRAL_MODULE_NUM  10

typedef enum{
    CENTRAL = 0,
    UART,
    IMU,
    BLE,
    ANT,
    RTC,
    CLI,
    TIME,
    SSPI
}MSG_ModuleType;

#ifdef ANT_ENABLE
#include <ant_stack_handler_types.h>
void ant_handler(ant_evt_t * p_ant_evt);
#define NUM_ANT_CHANNELS 2
#endif

