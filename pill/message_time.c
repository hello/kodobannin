#include <stddef.h>
#include <nrf_soc.h>
#include "app_timer.h"
#include "message_time.h"
#include "util.h"
#include "app.h"

static struct{
    MSG_Base_t base;
    bool initialized;
    struct hlo_ble_time ble_time;
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
static uint8_t 
_incCarry(uint8_t * target, uint8_t carry, uint8_t overflow){
    *target = *target + carry;
    if(*target > overflow){
        *target = 0;
        return 1;
    }
    return 0;
}
static void
_timer_handler(void * ctx){
    uint8_t carry;
    self.monotonic_time += 1000;
    TF_TickOneSecond();
    {
        struct hlo_ble_time t = self.ble_time;
        _incCarry(&t.year,
                _incCarry(&t.month,
                    _incCarry(&t.day,
                        _incCarry(&t.hours,
                            _incCarry(&t.minutes,
                                _incCarry(&t.seconds, 1, 59),59),23),31),12),9999);
        CRITICAL_REGION_ENTER();
        self.ble_time = t;
        CRITICAL_REGION_EXIT();
        TF_UpdateTime(&t);
    }
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
                PRINTS("SETTIME = ");
                self.monotonic_time = tmp->param.ble_time.monotonic_time;
                TF_Initialize(&tmp->param.ble_time);
                PRINT_HEX(&tmp->param.ble_time, sizeof(tmp->param.ble_time));
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
            self.initialized = 1;
            struct hlo_ble_time junk = {0};
            TF_Initialize(&junk);
        }
    }
    return &self.base;
}
MSG_Status MSG_Time_GetTime(struct hlo_ble_time * out_time){
    if(out_time){
        *out_time = self.ble_time;
        return SUCCESS;
    }else{
        return FAIL;
    }
}
MSG_Status MSG_Time_GetMonotonicTime(uint64_t * out_time){
    if(out_time){
        *out_time = self.monotonic_time;
        return SUCCESS;
    }else{
        return FAIL;
    }
}
