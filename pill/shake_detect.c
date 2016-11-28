#include "app.h"
#include "shake_detect.h"
#include "util.h"

static struct{
    uint32_t thmag, cnt;
    uint8_t shaking_time_sec;
    uint8_t factory_shake_cnt;
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

void ShakeDetectFactoryTest(void){
    self.factory_shake_cnt = 3;
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
                self._shake_detection_callback();
            }
            if(self.factory_shake_cnt){
                self.factory_shake_cnt--;
            }
        }
        _reset();
    }

}

bool ShakeDetect(uint32_t accelmag){
    uint32_t mag = self.thmag;
    if(self.factory_shake_cnt){
        mag = self.thmag / 2;
    }
    if(accelmag >= mag){
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
