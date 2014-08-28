#include "shake_detect.h"
#include "util.h"

#define SLIDING_WINDOW_SIZE_SEC         (5)
#define SHAKING_DATA_COUNT_THRESHOLD    (25)

static struct{
    uint32_t thmag, cnt;
    uint8_t shaking_time_sec;
}self;

static shake_detection_callback_t _shake_detection_callback;

void ShakeDetectReset(uint32_t threshold){
    self.thmag = threshold;
    self.cnt = 0;
    self.shaking_time_sec = 0;
}


void ShakeDetectDecWindow(void){

    if(self.shaking_time_sec){
        ++self.shaking_time_sec;
    }

    if(self.shaking_time_sec > SLIDING_WINDOW_SIZE_SEC){
        // sliding window ends, reset all the counter
        uint32_t data_above_threshold = self.cnt;
        self.shaking_time_sec = 0;
        self.cnt = 0;

        if(data_above_threshold >= SHAKING_DATA_COUNT_THRESHOLD){ // The user shakes hard enough and long enough
            if(_shake_detection_callback){
                _shake_detection_callback();
            }
        }
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
    _shake_detection_callback = callback;
}
