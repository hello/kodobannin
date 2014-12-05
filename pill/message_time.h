#pragma once
#include "message_base.h"
#include "hlo_ble_time.h"
#include "battery.h"

/**
 * time module, keeps track of time 
 * REQUIRES APP_TIMER to be initialized
 */

typedef struct{
    uint8_t (*cb)(const struct hlo_ble_time * time, uint64_t elapsed, void * ctx);
    void * ctx;
}MSG_TimeCB_t;
typedef struct{
    enum{
        TIME_PING=0,
        TIME_SYNC,
        TIME_STOP_PERIODIC,
        TIME_SET_1S_RESOLUTION,
        TIME_SET_5S_RESOLUTION,
        TIME_SET_WAKEUP_CB
    }cmd;
    union{
        struct hlo_ble_time ble_time;
        MSG_TimeCB_t wakeup_cb;
    }param;
}MSG_TimeCommand_t;

MSG_Base_t * MSG_Time_Init(const MSG_Central_t * central);
MSG_Status MSG_Time_GetMonotonicTime(uint64_t * out_time);
uint64_t* get_time();
void print_and_turn_off_adc(adc_t adc_result, uint32_t batt_level_milli_volts, uint8_t percentage_battery_level);



