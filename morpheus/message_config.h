#pragma once
/*
 * Here define the modules used
 */

#define ANT_ENABLE

#define MSG_BASE_SHARED_POOL_SIZE 12
#define MSG_BASE_DATA_BUFFER_SIZE 24

#define MSG_BASE_USE_BIG_POOL
#define MSG_BASE_SHARED_POOL_SIZE_BIG 6
#define MSG_BASE_DATA_BUFFER_SIZE_BIG 256


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
void ant_handler(void* event_data, uint16_t event_size);
#endif

#define MSG_CENTRAL_MODULE_NUM  (MOD_END)
