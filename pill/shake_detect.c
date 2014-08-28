#include "shake_detect.h"
#include "util.h"

static struct{
    uint32_t thmag, thcnt, cnt;
}self;

void ShakeDetectReset(uint32_t threshold, uint32_t count){
    self.thmag = threshold;
    self.thcnt = count;
    self.cnt = count;
}
void ShakeDetectDecWindow(void){
    self.cnt = self.cnt >> 1;
}
bool ShakeDetect(uint32_t accelmag){
    if(accelmag >= self.thmag){
        if(++self.cnt >= self.thcnt){
            self.cnt = 0;
            return true;
        }
    }
    return false;
}
