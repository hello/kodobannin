#pragma once

/**
 * a timed variant of a fifo in which index increases by time instead of data
 */

#include <stdint.h>
#include "hlo_ble_time.h"

/*
 * change this to be the unit of the fifo
 */

//time of each index
#define TF_UNIT_TIME_S 60 
#define TF_UNIT_TIME_MS 60000
#define TF_BUFFER_SIZE (8 * 60)
typedef int16_t tf_unit_t;  // Good job, this is a keen design!

typedef struct{
    uint8_t version;
    uint8_t reserved_1;
    uint16_t length;
    uint64_t mtime;
    uint16_t prev_idx;
    tf_unit_t data[TF_BUFFER_SIZE];
}__attribute__((packed)) tf_data_t;


void TF_Initialize(const struct hlo_ble_time * init_time);
void TF_TickOneSecond(const struct hlo_ble_time * init_time);
tf_unit_t TF_GetCurrent(void);
void TF_SetCurrent(tf_unit_t val);
tf_data_t * TF_GetAll(void);
