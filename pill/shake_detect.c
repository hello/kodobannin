#include "app.h"
#include "shake_detect.h"
#include "util.h"
#include "timedfifo.h"

static struct{
    uint32_t thmag, cnt;
    uint8_t shaking_time_sec;
    uint8_t last_shaking_time_sec;
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

void ShakeDetectDecWindow(void){

    //increase sliding window if started
    if(self.shaking_time_sec){
        ++self.shaking_time_sec;
    }
    //clear sliding window if time out
    if(self.shaking_time_sec > SLIDING_WINDOW_SIZE_SEC){
        // sliding window ends, reset all the counter
        if(self.cnt >= SHAKING_DATA_COUNT_THRESHOLD){ // The user shakes hard enough and long enough
            self.last_shaking_time_sec = get_tick();
            if(self._shake_detection_callback){
                self._shake_detection_callback();
            }
        }
        _reset();
    }

}

bool ShakeDetect(uint32_t accelmag){
    uint8_t current_time_sec = get_tick();
    int8_t diff = current_time_sec - self.last_shaking_time_sec;
    if(diff < 0)
    {
        diff = MOTION_DATA_INTERVAL_SEC - self.last_shaking_time_sec + current_time_sec;
    }
    // Do not detect shaking for 3 seconds after the previous shake is detected.
    if(current_time_sec - self.last_shaking_time_sec < MIN_SHAKE_DETECT_INTERVAL_SEC)
    {
        return false;
    }

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
