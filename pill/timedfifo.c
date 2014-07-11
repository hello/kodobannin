#include "timedfifo.h"
#include "util.h"
/**
typedef struct{
    uint8_t version;
    uint8_t reserved_1;
    uint16_t length;
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint8_t reserved_2;
    uint16_t year;
    tf_unit_t data[TF_BUFFER_SIZE];
}tf_data_t;

struct hlo_ble_time
{
    union {
        struct {
            uint16_t year; // 1582-9999
            uint8_t month; // 1-12, or 0 (unknown)
            uint8_t day; // 1-31, or 0 (unknown)
            uint8_t hours; // 0-23
            uint8_t minutes; // 0-59
            uint8_t seconds; // 0-59
            uint8_t weekday; // 0-7; 1-7=Mon-Sun, 0=Unknown
        } __attribute__((packed));
        uint8_t bytes[8];
    } __attribute__((packed));
} __attribute__((packed));
*/
static struct{
    tf_data_t data;
    uint16_t tickctr;
}self;
void TF_Initialize(const struct hlo_ble_time * init_time){
    self.data.length = sizeof(self);
    self.data.seconds = init_time->seconds;
    self.data.minutes = init_time->minutes;
    self.data.hours = init_time->hours;
    self.data.day = init_time->day;
    self.data.month = init_time->month;
    self.data.year = init_time->year;
    self.data.idx = 0;
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
