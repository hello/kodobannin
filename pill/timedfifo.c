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
    memset(&self.data, 0, sizeof(self.data));
    self.data.length = sizeof(self.data);
    self.data.prev_idx = 0xFFFF;
    
    self.current_idx = 0;
    self.tick = 0;
    self.data.mtime = init_time->monotonic_time;

    tf_unit_t* current = TF_GetCurrent();
    for (int i = 0; i < 3; i++) {
        current->min_accel[i] = INT16_MAX;
        current->max_accel[i] = INT16_MIN;
    }
    current->num_wakes = 0;

}



void TF_TickOneSecond(uint64_t monotonic_time){
    self.data.mtime = monotonic_time;
    if(++self.tick > MOTION_DATA_INTERVAL_SEC){
        self.tick = 0;
        self.data.prev_idx = self.current_idx;
        self.current_idx = (self.current_idx + 1) % TF_BUFFER_SIZE;
        //reset aux data struct

        tf_unit_t* current_slot = TF_GetCurrent();
        for (int i = 0; i < 3; i++) {
            current_slot->min_accel[i] = INT16_MAX;
            current_slot->max_accel[i] = INT16_MIN;
        }
        current_slot->num_wakes = 0;
        PRINTS("^");
    }else{
        PRINTS("*");
    }
}

inline tf_unit_t* TF_GetCurrent(void){
    return &self.data.data[self.current_idx];
}

inline void TF_SetCurrent(tf_unit_t* val){
    tf_unit_t* current = TF_GetCurrent();
    memcpy(current, val, sizeof(tf_unit_t));
}

inline tf_data_t * TF_GetAll(void){
    return &self.data;
}

static uint16_t _decrease_index(uint16_t * idx){
    if( *idx == 0){
        return TF_BUFFER_SIZE - 1;
    }else{
        return (*idx - 1);
    }
}


// for posterity -- this used to be used instead of dump payload
bool TF_GetCondensed(MotionPayload_t* payload, uint8_t length){
    bool has_data = false;

    if(payload){
        uint16_t idx = self.current_idx;
        //buf->time = self.data.mtime;

        for(int i = 0; i < length; i++){
            idx = _decrease_index(&idx);

            uint16_t maxrange = 0;
            uint16_t range = 0;
            int i = 0;
            tf_unit_t datum = self.data.data[idx];
            payload[i].num_times_woken_in_minute = datum.num_wakes;

            //compute max range
            for (int k = 0; k < 3; k++) {
                range = (uint16_t) (datum.max_accel[k] - datum.min_accel[k]);
                if (range > maxrange) {
                    maxrange = range;
                }
            } 

            payload[i].max_acc_range = maxrange;
            payload[i].maxaccnormsq = datum.max_amp; 

            if(datum.num_wakes != 0)
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

