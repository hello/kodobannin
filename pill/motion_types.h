#ifndef _MOTIONTYPES_H_
#define _MOTIONTYPES_H_

#include <stdint.h>


typedef struct {
    uint32_t maxaccnormsq;
    uint16_t max_acc_range; 
    uint64_t motion_mask;
} __attribute__((packed))  MotionPayload_t;

#endif
