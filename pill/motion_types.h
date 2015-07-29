#ifndef _MOTIONTYPES_H_
#define _MOTIONTYPES_H_

#include <stdint.h>


typedef struct {
    uint8_t max;
    uint8_t cos_theta;
    uint64_t motion_mask;
} __attribute__((packed))  MotionPayload_t;

#endif
