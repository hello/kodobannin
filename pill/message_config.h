#pragma once
/*
 * Here define the modules used
 */

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
    TIME
}MSG_ModuleType;

