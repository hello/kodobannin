#include "timedfifo.h"
#include "util.h"

static struct{
    tf_data_t data;
    uint16_t tickctr;
    uint16_t current_idx;
}self;

void TF_Initialize(const struct hlo_ble_time * init_time){
    memset(&self.data, 0, sizeof(self.data));
    self.data.length = sizeof(self.data);
    
    self.current_idx = 0;
    TF_UpdateTime(init_time);
}
void TF_UpdateTime(const struct hlo_ble_time * init_time){
    /*
    self.data.seconds = init_time->seconds;
    self.data.minutes = init_time->minutes;
    self.data.hours = init_time->hours;
    self.data.day = init_time->day;
    self.data.month = init_time->month;
    self.data.year = init_time->year;
    */
    self.data.mtime = init_time->monotonic_time;
}
void TF_TickOneSecond(void){
    PRINTS("*");
    if(++self.tickctr >= TF_UNIT_TIME_S ){
        if(self.data.mtime >= TF_UNIT_TIME_S * 1000){
            self.data.mtime -= TF_UNIT_TIME_S * 1000;
            self.data.prev_idx = self.current_idx;
        }
        self.tickctr = 0;
        self.current_idx = (self.current_idx + 1) % TF_BUFFER_SIZE;
        self.data.data[self.current_idx] = 0;
        MSG_Time_GetMonotonicTime(&self.data.mtime);
        PRINTS("^");
    }
}
tf_unit_t TF_GetCurrent(void){
    return self.data.data[self.current_idx];
}
void TF_SetCurrent(tf_unit_t val){
    self.data.data[self.current_idx] = val;
}
tf_data_t * TF_GetAll(void){
    return &self.data;
}
