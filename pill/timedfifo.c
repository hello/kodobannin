#include <string.h>
#include "platform.h"
#include "app.h"

#include "timedfifo.h"
#include "util.h"

static struct{
    tf_data_t data;
    uint8_t tick;
    uint16_t current_idx;
}self;

static uint16_t _decrease_index(uint16_t * idx);

void TF_Initialize(const struct hlo_ble_time * init_time){
    uint16_t i;
    memset(&self.data, 0, sizeof(self.data));
    self.data.length = sizeof(self.data);
    self.data.prev_idx = 0xFFFF;
    
    self.current_idx = 0;
    self.tick = 0;
    self.data.mtime = init_time->monotonic_time;

    for (i = 0; i < 3; i++) {
        self.data.aux_data.min_accel[i] = INT16_MAX;
        self.data.aux_data.max_accel[i] = INT16_MIN;
    }
    self.data.aux_data.num_wakes = 0;

}



void TF_TickOneSecond(uint64_t monotonic_time){
    self.data.mtime = monotonic_time;
    if(++self.tick > MOTION_DATA_INTERVAL_SEC){
        self.tick = 0;
        self.data.prev_idx = self.current_idx;
        self.current_idx = (self.current_idx + 1) % TF_BUFFER_SIZE;
        self.data.data[self.current_idx] = 0;
        PRINTS("^");
    }else{
        PRINTS("*");
    }
}

inline tf_unit_t TF_GetCurrent(void){
    return self.data.data[self.current_idx];
}

inline void TF_SetCurrent(tf_unit_t val){
    self.data.data[self.current_idx] = val;
}

inline void TF_IncrementWakeCounts(void) {
   self.data.aux_data.num_wakes++;
}

inline auxillary_data_t * TF_GetAuxData(void) {
    return &self.data.aux_data;
}

inline tf_data_t * TF_GetAll(void){
    return &self.data;
}
static
uint16_t _decrease_index(uint16_t * idx){
    if( *idx == 0){
        return TF_BUFFER_SIZE - 1;
    }else{
        return (*idx - 1);
    }
}

bool TF_DumpPayload(MotionPayload_t * payload) {
    int16_t * max = self.data.aux_data.max_accel;
    int16_t * min = self.data.aux_data.min_accel;
    uint16_t i;
    uint16_t maxrange = 0;
    uint16_t range;
    uint16_t idx = self.current_idx;

    //when will this ever happen?
    if (!payload) {
        return false;
    }

    //never woke up
    if (self.data.aux_data.num_wakes == 0) {
        return false;
    }

    payload->num_times_woken_in_minute = self.data.aux_data.num_wakes;

    //compute max range
    for (i = 0; i < 3; i++) {
        range = (uint16_t) (max[i] - min[i]);
        if (range > maxrange) {
            maxrange = range;
        }
    } 

    payload->max_acc_range = maxrange;

    //get normal payload
    idx = _decrease_index(&idx);
    payload->maxaccnormsq =  self.data.data[idx]; 

    //reset aux data struct
    for (i = 0; i < 3; i++) {
        min[i] = INT16_MAX;
        max[i] = INT16_MIN;
    }
    self.data.aux_data.num_wakes = 0;

    return true;
}

// for posterity -- this used to be used instead of dump payload
bool TF_GetCondensed(uint32_t* buf, uint8_t length){
    bool has_data = false;

    if(buf){
        uint16_t  i,idx = self.current_idx;
        //buf->time = self.data.mtime;

        for(i = 0; i < length; i++){
            idx = _decrease_index(&idx);
            buf[i] = self.data.data[idx];
            if(self.data.data[idx] != 0)
            {
                has_data = true;
            }
        }
    }

    return has_data;
}

inline uint8_t get_tick(){
    return self.tick;
}

