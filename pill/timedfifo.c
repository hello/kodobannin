#include "timedfifo.h"
#include "util.h"

static struct{
    tf_data_t data;
    uint16_t tickctr;
}self;

void TF_Initialize(const struct hlo_ble_time * init_time){
    self.data.length = sizeof(self.data);
    self.data.idx = 0;
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
        self.tickctr = 0;
        self.data.idx = (self.data.idx + 1) % TF_BUFFER_SIZE;
        self.data.data[self.data.idx] = 0;
        PRINTS("^");
    }
}
tf_unit_t TF_GetCurrent(void){
    return self.data.data[self.data.idx];
}
void TF_SetCurrent(tf_unit_t val){
    self.data.data[self.data.idx] = val;
}
tf_data_t * TF_GetAll(void){
    return &self.data;
}
