#ifndef _MOTIONTYPES_H_
#define _MOTIONTYPES_H_

#include <stdint.h>

typedef struct {
    uint32_t maxaccnorm;
    uint16_t max_acc_range; 
    uint16_t num_times_woken_in_minute;
} MotionPayload_t; 

#endif
