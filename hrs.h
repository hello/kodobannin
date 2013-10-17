#pragma once

#define HRS_MASK (~0x7)
#define NUM_BUCKETS 32
#define MAX_VALUE 0xFF
#define BUCKET_LVL ((MAX_VALUE+1)/NUM_BUCKETS)

void adc_test();