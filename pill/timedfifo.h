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
#define TF_BUFFER_SIZE (8 * 60)
typedef uint16_t tf_unit_t;

typedef struct{
    uint8_t version;
    uint8_t reserved_1;
    uint16_t length;
    uint64_t mtime;
    uint16_t idx;
    tf_unit_t data[TF_BUFFER_SIZE];
}tf_data_t;


void TF_Initialize(const struct hlo_ble_time * init_time);
void TF_UpdateTime(const struct hlo_ble_time * new_time);
void TF_TickOneSecond(void);
tf_unit_t TF_GetCurrent(void);
void TF_SetCurrent(tf_unit_t val);
tf_data_t * TF_GetAll(void);
