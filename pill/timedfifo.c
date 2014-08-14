#include "timedfifo.h"
#include "util.h"

static struct{
    tf_data_t data;
    uint16_t current_idx;
}self;

static int16_t _raw_xyz[3];

void TF_Initialize(const struct hlo_ble_time * init_time){
    memset(&self.data, 0, sizeof(self.data));
    self.data.length = sizeof(self.data);
    self.data.prev_idx = 0xFFFF;
    
    self.current_idx = 0;
    self.data.mtime = init_time->monotonic_time;
}


void TF_TickOneSecond(const struct hlo_ble_time * current_time){
    
    PRINTS("*");
    if(current_time->monotonic_time % TF_UNIT_TIME_MS == 0){
        self.data.mtime = current_time->monotonic_time - TF_UNIT_TIME_MS;
        self.data.prev_idx = self.current_idx;
        self.current_idx = (self.current_idx + 1) % TF_BUFFER_SIZE;
        self.data.data[self.current_idx] = 0;
        //MSG_Time_GetMonotonicTime(&self.data.mtime);
        PRINTS("^");
    }
}

inline tf_unit_t TF_GetCurrent(void){
    return self.data.data[self.current_idx];
}

inline void TF_SetCurrent(tf_unit_t val){
    self.data.data[self.current_idx] = val;
}

inline tf_data_t * TF_GetAll(void){
    return &self.data;
}

inline int16_t* get_raw_xzy_address()
{
    return _raw_xyz;
}
