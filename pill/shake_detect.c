#include "shake_detect.h"
#include "util.h"

#define SLIDING_WINDOW_SIZE_SEC         (5)
#define SHAKING_DATA_COUNT_THRESHOLD    (25)

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


void ShakeDetectDecWindow(void){

    //increase sliding window if started
    if(self.shaking_time_sec){
        ++self.shaking_time_sec;
    }
    //shake immediately if above th
    if(self.cnt >= SHAKING_DATA_COUNT_THRESHOLD){ // The user shakes hard enough and long enough
        if(self._shake_detection_callback){
            self._shake_detection_callback();
        }
        _reset();
    }
    //clear sliding window if time out
    if(self.shaking_time_sec > SLIDING_WINDOW_SIZE_SEC){
        // sliding window ends, reset all the counter
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
