#pragma once

/**
 * a timed variant of a fifo in which index increases by time instead of data
 */

#include <stdint.h>

/*
 * change this to be the unit of the fifo
 */

//time of each index
#define TF_UNIT_TIME_MS 60000 
#define TF_BUFFER_SIZE (2 * 60)
typedef uint16_t tf_unit_t;

void TF_Initialize(uint64_t initial_time);
uint16_t TF_UpdateIdx(uint64_t current_time);
tf_unit_t * TF_GetCurrent(void);
