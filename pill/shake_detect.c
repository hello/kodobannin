#include "shake_detect.h"
#include "util.h"

#define SLIDING_WINDOW_SIZE_SEC         (2)
#define SHAKING_DATA_COUNT_THRESHOLD    (8)

static struct{
    uint32_t thmag, cnt;
    uint8_t shaking_time_sec;
    shake_detection_callback_t _shake_detection_callback;
}self;

static inline void _reset(void){
    self.cnt = 0;
    self.shaking_time_sec = 0;
}
void ShakeDetectReset(uint32_t threshold){
    self.thmag = threshold;
    _reset();
}

static inline void _on_shake(void * p_event_data, uint16_t event_size)
{
    PRINTS("On shake\r\n");
    self._shake_detection_callback();
}

void ShakeDetectDecWindow(void){

    //increase sliding window if started
    if(self.shaking_time_sec){
        ++self.shaking_time_sec;
    }
    //clear sliding window if time out
    if(self.shaking_time_sec > SLIDING_WINDOW_SIZE_SEC){
        // sliding window ends, reset all the counter
        if(self.cnt >= SHAKING_DATA_COUNT_THRESHOLD){ // The user shakes hard enough and long enough
            if(self._shake_detection_callback){
                app_sched_event_put(NULL, 0, _on_shake);
                //self._shake_detection_callback();
            }
        }
        _reset();
    }

}

bool ShakeDetect(uint32_t accelmag){
    if(accelmag >= self.thmag){
        if(!self.shaking_time_sec){
            ++self.shaking_time_sec;  // initialize the sliding window timer.
        }
        ++self.cnt;
        return true;
    }

    return false;
}

void set_shake_detection_callback(shake_detection_callback_t callback){
    self._shake_detection_callback = callback;
}
