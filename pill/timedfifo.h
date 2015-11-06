#pragma once
#include "motion_types.h"

/**
 * a timed variant of a fifo in which index increases by time instead of data
 */

#include <stdint.h>
#include <stdbool.h>

#include "platform.h"
#include "app.h"
#include "hlo_ble_time.h"


typedef struct {
    uint32_t max_amp;
    int16_t avg_accel[3];
    int16_t prev_avg_accel[3];
    uint64_t motion_mask;
    uint32_t num_meas;
    uint8_t has_motion;
}__attribute__((packed)) tf_unit_t;

void TF_Initialize();
void TF_TickOneMinute(void);
tf_unit_t* TF_GetCurrent(void);
bool TF_GetCondensed(MotionPayload_t* buf);
