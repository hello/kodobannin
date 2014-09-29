#include <stddef.h>
#include <nrf_soc.h>
#include "app_timer.h"
#include "message_time.h"
#include "util.h"
#include "platform.h"
#include "app.h"
#include "shake_detect.h"
#include "timedfifo.h"

#ifdef ANT_STACK_SUPPORT_REQD
#include "message_ant.h"
#endif


#define HEARTBEAT_INTERVAL_SEC    (3600)

static struct{
    MSG_Base_t base;
    bool initialized;
    struct hlo_ble_time ble_time;  // Keep it here for debug, can see if pill crashed or not.
    MSG_Central_t * central;
    app_timer_id_t timer_id;
    MSG_Data_t * user_cb;
    uint32_t uptime;
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

#ifdef ANT_STACK_SUPPORT_REQD
static void _send_available_data_ant(){
    MSG_Data_t* data_page = MSG_Base_AllocateDataAtomic(sizeof(MSG_ANT_PillData_t) + TF_CONDENSED_BUFFER_SIZE);
    if(data_page){
        memset(&data_page->buf, 0, sizeof(data_page->len));
        MSG_ANT_PillData_t* ant_data = &data_page->buf;
        ant_data->version = 0x1;
        ant_data->type = ANT_PILL_DATA;
        ant_data->UUID = GET_UUID_64();
        ant_data->payload_len = TF_CONDENSED_BUFFER_SIZE;

        if(TF_GetCondensed(ant_data->payload, TF_CONDENSED_BUFFER_SIZE))
        {
          self.central->dispatch((MSG_Address_t){TIME,1}, (MSG_Address_t){ANT,1}, data_page);
        }else{
            // Morpheus should fake data if there is nothing received for that minute.
        }
        MSG_Base_ReleaseDataAtomic(data_page);
    }
}


static void _send_heartbeat_data_ant(){
    // TODO: Jackson please do review & test this.
    // I packed all the struct and I am not sure it will work as expected.
    MSG_Data_t* data_page = MSG_Base_AllocateDataAtomic(sizeof(MSG_ANT_PillData_t));
    if(data_page){
        pill_heartbeat_t heartbeat = (pill_heartbeat_t){
            .battery_level = 100,
            .uptime_sec = self.uptime
        };
        memset(&data_page->buf, 0, sizeof(data_page->len) + sizeof(pill_heartbeat_t));
        MSG_ANT_PillData_t* ant_data = &data_page->buf;
        ant_data->version = 0x1;
        ant_data->type = ANT_PILL_HEARTBEAT;
        ant_data->UUID = GET_UUID_64();
        memcpy(ant_data->payload, &heartbeat, sizeof(heartbeat));
        self.central->dispatch((MSG_Address_t){TIME,1}, (MSG_Address_t){ANT,1}, data_page);
        MSG_Base_ReleaseDataAtomic(data_page);
    }
}

#endif

static void _timer_handler(void * ctx){
    //uint8_t carry;
    self.ble_time.monotonic_time += 1000;  // Just keep it for current data collection task.
    self.uptime += 1;

    TF_TickOneSecond(self.ble_time.monotonic_time);
#ifdef ANT_ENABLE
    if(get_tick() == 0)
    {
        _send_available_data_ant();
    }


    if(self.uptime % HEARTBEAT_INTERVAL_SEC == 0)
    {
        _send_heartbeat_data_ant();
    }
    ShakeDetectDecWindow();
#endif
    
    if(self.user_cb){
        MSG_TimeCB_t * cb = &((MSG_TimeCommand_t*)(self.user_cb->buf))->param.wakeup_cb;
        if(cb->cb(&self.ble_time,1000,cb->ctx)){
            MSG_Base_ReleaseDataAtomic(self.user_cb);
            self.user_cb = NULL;
        }
    }
}

static MSG_Status _send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
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
                //TF_Initialize(&tmp->param.ble_time);
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
            case TIME_SET_5S_RESOLUTION:
                PRINTS("PERIODIC 5S");
                ticks = APP_TIMER_TICKS(5000,APP_TIMER_PRESCALER);
                app_timer_stop(self.timer_id);
                app_timer_start(self.timer_id, ticks, NULL);
                break;
            case TIME_SET_WAKEUP_CB:
                MSG_Base_AcquireDataAtomic(data);
                MSG_Base_ReleaseDataAtomic(self.user_cb);
                self.user_cb = data;
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
        self.uptime = 0;

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

uint64_t* get_time()
{
    return &self.ble_time.monotonic_time;
}
