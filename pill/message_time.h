#pragma once
#include "message_base.h"
#include "hlo_ble_time.h"

/**
 * time module, keeps track of time 
 * REQUIRES APP_TIMER to be initialized
 */


typedef enum{
    MSG_TIME_PING = 0,
    MSG_TIME_SYNC,
    MSG_TIME_STOP_PERIODIC,
    MSG_TIME_SET_1S_RESOLUTION,
}MSG_Time_Commands;

MSG_Base_t * MSG_Time_Init(const MSG_Central_t * central);
MSG_Status MSG_Time_GetMonotonicTime(uint64_t * out_time);
uint64_t* get_time();



