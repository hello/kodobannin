#include "time_keeper.h"
#include "app_timer.h"
#include "app.h"
#include "util.h"

static app_timer_id_t timer;
static uint32_t user_time;
static uint32_t uptime;

static void _time_keeper_handler(void * ctx){
    if(user_time){
        user_time++;
    }
    uptime++;
}

void time_keeper_init(void){
    uint32_t ret;
    uint32_t ticks;
    ret = app_timer_create(&timer, APP_TIMER_MODE_REPEATED,_time_keeper_handler);
    APP_OK(ret);
    ticks = APP_TIMER_TICKS(1000,APP_TIMER_PRESCALER);
    ret = app_timer_start(timer, ticks, NULL);
    APP_OK(ret);
}

void time_keeper_set(uint32_t utime){
    user_time = utime;
}

uint32_t time_keeper_get(void){
    return user_time;
}
uint32_t time_keeper_uptime(void){
    return uptime;
}
