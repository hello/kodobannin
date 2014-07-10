#include <stddef.h>
#include "app_timer.h"
#include "message_time.h"
#include "util.h"
#include "app.h"

static struct{
    MSG_Base_t base;
    bool initialized;
    struct rtc_time_t time;
    uint64_t monotonic_time;//used for internal datastructure
    MSG_Central_t * central;
    app_timer_id_t timer_id;
}self;

static char * name = "TIME";

static MSG_Status
_init(void){
    return SUCCESS;
}

static MSG_Status
_destroy(void){
    return SUCCESS;
}

static MSG_Status
_flush(void){
    return SUCCESS;
}
static void
_timer_handler(void * ctx){
    self.monotonic_time += 1000;
    //in the future tf should be handled in a separate module
    TF_UpdateIdx(self.monotonic_time);
    PRINT_HEX(&self.monotonic_time, sizeof(self.monotonic_time));
}

static MSG_Status
_send(MSG_ModuleType src, MSG_Data_t * data){
    if(data){
        uint32_t ticks;
        MSG_Base_AcquireDataAtomic(data);
        MSG_TimeCommand_t * tmp = data->buf;
        switch(tmp->cmd){
            default:
            case TIME_PING:
                PRINTS("TIMEPONG");
                break;
            case TIME_SYNC:
                self.time = tmp->param.time;
                PRINT_HEX(&tmp->param.time, sizeof(tmp->param.time));
                break;
            case TIME_STOP_PERIODIC:
                PRINTS("STOP_HEARTBEAT");
                app_timer_stop(self.timer_id);
                break;
            case TIME_SET_1S_RESOLUTION:
                PRINTS("PERIODIC 1S");
                ticks = APP_TIMER_TICKS(1000,APP_TIMER_PRESCALER);
                app_timer_stop(self.timer_id);
                app_timer_start(self.timer_id, ticks, NULL);
                break;
            case TIME_SET_5S_RESOLUTION:
                PRINTS("PERIODIC 1S");
                ticks = APP_TIMER_TICKS(5000,APP_TIMER_PRESCALER);
                app_timer_stop(self.timer_id);
                app_timer_start(self.timer_id, ticks, NULL);
                break;
        }

        MSG_Base_ReleaseDataAtomic(data);
    }
    return SUCCESS;
}

MSG_Base_t * MSG_Time_Init(const MSG_Central_t * central){
    if(!self.initialized){
        self.base.init = _init;
        self.base.destroy = _destroy;
        self.base.flush = _flush;
        self.base.send = _send;
        self.base.type = TIME;
        self.base.typestr = name;
        self.central = central;
        self.monotonic_time = 0;
        if(app_timer_create(&self.timer_id,APP_TIMER_MODE_REPEATED,_timer_handler) == NRF_SUCCESS){
            TF_Initialize(self.monotonic_time);
            self.initialized = 1;
        }
    }
    return &self.base;
}
const struct rtc_time_t * MSG_Time_GetTime(void){
    return &self.time;
}
