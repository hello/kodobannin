#include <string.h>
#include "platform.h"
#include "app.h"
#include "app_timer.h"

#include "timedfifo.h"
#include "util.h"

static struct{
    tf_data_t data;
    uint8_t tick;
    uint16_t current_idx;
}self;

static uint16_t _decrease_index(uint16_t * idx);

static void
_reset_tf_unit(tf_unit_t * current){
    for (int i = 0; i < 3; i++) {
        current->min_accel[i] = INT16_MAX;
        current->max_accel[i] = INT16_MIN;
    }
    current->has_motion = 0;
    current->num_wakes = 0;
    current->max_amp = 0;
    current->duration = 0;
}
void TF_Initialize(){
    memset(&self.data, 0, sizeof(self.data));
    self.data.length = sizeof(self.data);
    self.data.prev_idx = 0xFFFF;
    
    self.current_idx = 0;
    self.tick = 0;

    tf_unit_t* current = TF_GetCurrent();
    _reset_tf_unit(current);
}

void TF_TickOneMinute() {
    tf_unit_t* current_slot = TF_GetCurrent();
    //increment index
    self.tick = 0;
    self.data.prev_idx = self.current_idx;
    self.current_idx = (self.current_idx + 1) % TF_BUFFER_SIZE;
    
    //reset next tf data
    _reset_tf_unit(current_slot);
    PRINTS("^");
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

    if(payload && length){
        uint16_t idx = self.current_idx;

        for(int i = 0; i < length; i++){
            uint16_t maxrange = 0;
            uint16_t range = 0;
            tf_unit_t datum = self.data.data[idx];

            uint8_t payload_index = length - i - 1;
            payload->num_times_woken_in_minute = (uint8_t)datum.num_wakes;

            //compute max range
            for (int k = 0; k < 3; k++) {
                range = (uint16_t) (datum.max_accel[k] - datum.min_accel[k]);
                if (range > maxrange) {
                    maxrange = range;
                }
            } 

            payload[payload_index].max_acc_range = maxrange;
            payload[payload_index].maxaccnormsq = datum.max_amp;

            //convert to seconds rounding up
            payload[payload_index].duration =  ( ( datum.duration / APP_TIMER_TICKS( 500, APP_TIMER_PRESCALER ) ) + 1 ) / 2;

            if(datum.num_wakes != 0)
            {
                has_data = true;
            }
            idx = _decrease_index(&idx);
        }
    }

    return has_data;
}

inline uint8_t get_tick(){
    return self.tick;
}

