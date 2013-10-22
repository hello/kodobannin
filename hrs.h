#pragma once

#define HRS_MASK (~0x7)
#define NUM_BUCKETS 16
#define MAX_VALUE 0xFF
#define BUCKET_LVL ((MAX_VALUE+1)/NUM_BUCKETS)

typedef void (*hrs_send_data_cb)(const uint8_t *data, const uint16_t len);

void adc_test();

void hrs_calibrate(uint8_t power_lvl_min, uint8_t power_lvl_max, uint16_t delay, uint16_t samples, hrs_send_data_cb data_send);
//uint32_t hrs_calibrate();
void hrs_run_test( uint8_t power_lvl, uint16_t delay, uint16_t samples, bool keep_the_lights_on);
uint8_t * get_hrs_buffer();
