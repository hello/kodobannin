#include <string.h>
#include "platform.h"
#include "app.h"
#include "app_timer.h"

#include "timedfifo.h"
#include "util.h"

static struct{
    tf_unit_t data;
}self;

static void
_reset_tf_unit(tf_unit_t * current){
    for (int i = 0; i < 3; i++) {
        current->prev_avg_accel[i] = current->avg_accel[i];
        current->avg_accel[i] = 0;
    }
    current->has_motion = 0;
    current->motion_mask = 0;
    current->max_amp = 0;
}
void TF_Initialize(){
    memset(&self.data, 0, sizeof(self.data));
    _reset_tf_unit(&self.data);
}

void TF_TickOneMinute() {
    if( self.data.motion_mask ) {
        _reset_tf_unit(&self.data);
    }
    PRINTS("^");
}

tf_unit_t * TF_GetCurrent(void){
    return &self.data;
}
#define PRINT_HEX_X(x) PRINT_HEX(&x, sizeof(x)); PRINTS("\r\n");

//assumes result is near 1 in 16.16
static uint32_t _fastinvsqrt( uint32_t x ) {
    uint32_t half = 1<<15;
    uint32_t one = 1<<16;
    
    x = (uint32_t)(((uint64_t) 200 * ( (one|half) - (((uint64_t)x * x)>>17)))>>16);
    x = (uint32_t)(((uint64_t)x * ( (one|half) - (((uint64_t)x * x)>>17)))>>16);
    x = (uint32_t)(((uint64_t)x * ( (one|half) - (((uint64_t)x * x)>>17)))>>16); //thanks quake - http://betterexplained.com/articles/understanding-quakes-fast-inverse-square-root/
    return x;
}
static uint32_t _mag( uint16_t * x ) {
    uint32_t m=0;
    for (int k = 0; k < 3; k++) {
        m += ((uint32_t)x[k]*x[k])>>16;
    }
    return m;
}

static uint32_t _fastsqrt( uint32_t x ) {
    int64_t s = x;
    x = 20000u;
    x = ( x + s / x )/2;
    x = ( x + s / x )/2;
    x = ( x + s / x )/2;
    x = ( x + s / x )/2;
    return x;
}

static uint8_t _bitlog(uint32_t n) {
    int16_t b;
    
    // shorten computation for small numbers
    if(n <= 8)
        return (int16_t)(2 * n);
    
    // find the highest non-zero bit
    b=31;
    while((b > 2) && ((int32_t)n > 0))
    {
        --b;
        n <<= 1;
    }
    n &= 0x70000000;
    n >>= 28; // keep top 4 bits, of course we're only using 3 of those
    
    b = (int16_t)n + 8 * (b - 1);
    return (uint8_t) b;
}


// for posterity -- this used to be used instead of dump payload
bool TF_GetCondensed(MotionPayload_t* payload){
    bool has_data = false;
    tf_unit_t datum = self.data;
    
    if(payload && datum.motion_mask ){
        int32_t dot = 0;

        //compute max range
        for (int k = 0; k < 3; k++) {
            dot +=(datum.avg_accel[k]*datum.prev_avg_accel[k]);
        }
        dot = abs(dot);
        dot = ((uint64_t)dot*_fastinvsqrt(_mag(datum.avg_accel)))>>16;
        dot = ((uint64_t)dot*_fastinvsqrt(_mag(datum.prev_avg_accel)))>>16;
        
        payload->cos_theta = _bitlog(dot);
        uint32_t s = _fastsqrt(datum.max_amp);
        if ( s < IMU_ONE_G ) {
            payload->max = 0;
        } else {
            s = (s-IMU_ONE_G)>>7; //remove 1g, scale into 8 bits
            if( s < UINT8_MAX ) {
                payload->max = s;
            }else{
                payload->max = UINT8_MAX;
            }
        }
        payload->motion_mask = datum.motion_mask;
#if 1
        PRINTF("data\n cos_theta: %d max: %d (%d) mask: %l\r\n", payload->cos_theta, payload->max, s, payload->motion_mask );
#endif
        
        has_data = true;
    }

    return has_data;
}

