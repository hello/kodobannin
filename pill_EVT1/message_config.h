#pragma once
/*
 * Here define the modules used
 */

#define MSG_BASE_SHARED_POOL_SIZE 10
#define MSG_BASE_DATA_BUFFER_SIZE 31

#define MSG_CENTRAL_MODULE_NUM  10

typedef enum{
    CENTRAL = 0,
    UART,
    SPI,
    BTLE,
    ANT,
    RTC,
    CLI
}MSG_ModuleType;

