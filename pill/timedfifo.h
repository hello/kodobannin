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

/*
 * change this to be the unit of the fifo
 */

//time of each index
#define TF_UNIT_TIME_S 60 
#define TF_UNIT_TIME_MS 60000

#ifdef DATA_SCIENCE_TASK
#define TF_BUFFER_SIZE (30)
#else
#define TF_BUFFER_SIZE (10)
#endif

typedef struct {
    uint32_t max_amp;
    uint16_t num_wakes;
    int16_t max_accel[3];
    int16_t min_accel[3];
    uint32_t duration;
    uint8_t has_motion;
} auxillary_data_t;

typedef auxillary_data_t tf_unit_t;

typedef struct{
    uint8_t version;
    uint8_t reserved_1;
    uint16_t length;
    uint16_t prev_idx;
    tf_unit_t data[TF_BUFFER_SIZE];
} tf_data_t;


void TF_Initialize();
void TF_TickOneMinute(void);
tf_unit_t* TF_GetCurrent(void);
void TF_SetCurrent(tf_unit_t* val);
tf_data_t * TF_GetAll(void);
bool TF_GetCondensed(MotionPayload_t* buf, uint8_t length);
uint8_t get_tick();
