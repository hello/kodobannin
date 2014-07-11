#pragma once
#include "message_base.h"
#include "hlo_ble_time.h"

/**
 * time module, keeps track of time 
 * REQUIRES APP_TIMER to be initialized
 */

typedef struct{
    enum{
        TIME_PING=0,
        TIME_SYNC,
        TIME_STOP_PERIODIC,
        TIME_SET_1S_RESOLUTION,
        TIME_SET_5S_RESOLUTION
    }cmd;
    union{
        struct hlo_ble_time ble_time;
    }param;
}MSG_TimeCommand_t;

MSG_Base_t * MSG_Time_Init(const MSG_Central_t * central);
MSG_Status MSG_Time_GetTime(struct hlo_ble_time * out_time);



