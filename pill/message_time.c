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
    //uint8_t carry;
    self.ble_time.monotonic_time += 1000;
    TF_TickOneSecond(&self.ble_time);
}

static MSG_Status
_send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
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
                self.ble_time.monotonic_time = tmp->param.ble_time.monotonic_time;
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
        memset(&self.ble_time, 0, sizeof(self.ble_time));
        
        if(app_timer_create(&self.timer_id,APP_TIMER_MODE_REPEATED,_timer_handler) == NRF_SUCCESS){
            self.initialized = 1;
            TF_Initialize(&self.ble_time);
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
        *out_time = self.ble_time.monotonic_time;
        return SUCCESS;
    }else{
        return FAIL;
    }
}
