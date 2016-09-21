#include "app.h"
#include "shake_detect.h"
#include "util.h"

#define MAX_COUNTS_BETWEEN_EVENTS (1000)

static struct{
    uint32_t thmag, cnt;
    uint32_t last_time;
    shake_detection_callback_t _shake_detection_callback;
}self;

static inline void _reset(void){
    self.cnt = 0;
    self.last_time = 0;
}


void ShakeDetectReset(uint32_t threshold){
     self.thmag = threshold;
    _reset();
}


bool ShakeDetect(uint32_t accelmag, uint32_t time){
    if(accelmag < self.thmag){
        return false;
    }
    
    if (time - self.last_time > MAX_COUNTS_BETWEEN_EVENTS) {
        self.cnt = 0;
    }

    self.last_time = time;

    if (++self.cnt > SHAKING_DATA_COUNT_THRESHOLD && self._shake_detection_callback) {
        self._shake_detection_callback(); 
    }


    return true;
}



void set_shake_detection_callback(shake_detection_callback_t callback){
    self._shake_detection_callback = callback;
}
