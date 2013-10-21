#pragma once

#define HRS_MASK (~0x7)
#define NUM_BUCKETS 16
#define MAX_VALUE 0xFF
#define BUCKET_LVL ((MAX_VALUE+1)/NUM_BUCKETS)

void adc_test();

uint32_t hrs_calibrate();
void hrs_run_test( uint8_t power_lvl, uint16_t delay, uint16_t samples);
uint8_t * get_hrs_buffer();
