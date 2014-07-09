#pragma once
#include "message_base.h"
#include "rtc.h"

/**
 * time module, keeps track of time 
 * REQUIRES APP_TIMER to be initialized
 */

typedef struct{
    enum{
        PING=0,
        SYNC,
        STOP_PERIODIC,
        SET_1S_RESOLUTION,
        SET_5S_RESOLUTION
    }cmd;
    union{
        struct rtc_time_t time;
    }param;
}MSG_TimeCommand_t;

MSG_Base_t * MSG_Time_Init(const MSG_Central_t * central);
const struct rtc_time_t * MSG_Time_GetTime(void);




