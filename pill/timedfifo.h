#pragma once

/**
 * a timed variant of a fifo in which index increases by time instead of data
 */

#include <stdint.h>

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
#define TF_BUFFER_SIZE (9 * 60)
#else
#define TF_BUFFER_SIZE (2 * 60)
#endif

#define TF_CONDENSED_BUFFER_SIZE (3)

typedef int32_t tf_unit_t;  // Good job, this is a keen design!

typedef struct{
    uint8_t version;
    uint8_t reserved_1;
    uint16_t length;
    uint64_t mtime;
    uint16_t prev_idx;
    tf_unit_t data[TF_BUFFER_SIZE];
}__attribute__((packed)) tf_data_t;

typedef struct{
    uint8_t version;
    uint8_t reserved[3];
    uint64_t UUID;
    uint64_t time;
    tf_unit_t data[TF_CONDENSED_BUFFER_SIZE];
}__attribute__((packed)) tf_data_condensed_t;


void TF_Initialize(const struct hlo_ble_time * init_time);
void TF_TickOneSecond(uint64_t monotonic_time);
tf_unit_t TF_GetCurrent(void);
void TF_SetCurrent(tf_unit_t val);
tf_data_t * TF_GetAll(void);
void TF_GetCondensed(tf_data_condensed_t * buf);
