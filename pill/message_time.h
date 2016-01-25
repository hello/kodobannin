#pragma once
#include "message_base.h"
#include "hlo_ble_time.h"

/**
 * time module, keeps track of time 
 * REQUIRES APP_TIMER to be initialized
 */


typedef enum{
    MSG_TIME_PING = 0,
    MSG_TIME_STOP_PERIODIC,
    MSG_TIME_SET_START_1SEC,
    MSG_TIME_SET_START_1MIN,
}MSG_Time_Commands;

MSG_Base_t * MSG_Time_Init(const MSG_Central_t * central);

