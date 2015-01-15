//
//  HEHeartRateAnalysis.c
//  Senkusha
//
//  Created by André Pang on 10/23/13.
//  Copyright (c) 2013 Hello Inc. All rights reserved.
//

#ifdef HRA_TEST
#include <stdio.h>
#include <math.h>
#endif

#include "peak_detect.h"

void pre_analysis(const sample_t* const begin, const sample_t* const end, sample_t* out_min_value, sample_t* out_max_value)
{
    int i;
    const sample_t* p;

    sample_t max = 0;
    sample_t min = UINT8_MAX;

    for(p = begin, i = 0; p < end; p++, i++) {
        const sample_t sample = *p;

        if(sample > max) {
            max = sample;
        }

        if(sample < min) {
            min = sample;
        }
    }

    *out_min_value = min;
    *out_max_value = max;
}

peaks_t detect_peaks(const sample_t* const begin, const sample_t* const end, const sample_rate_t hz)
{
    static const size_t WIDTH = 10;

    sample_t min_val, max_val;
    pre_analysis(begin, begin+hz, &min_val, &max_val);

#ifdef HRA_TEST
    printf("MIN, MAX %hhu %hhu\n", min_val, max_val);
#endif

    sample_t found_min_val = max_val;
    size_t found_min_counts = 0;

    uint8_t detected = 0;

    const sample_t* p;
    for(p = begin+hz; p < end; p++) {
        const sample_t sample = *p;

        if(sample > min_val) {
            if(found_min_counts >= WIDTH && sample > 30) {
                detected++;
                found_min_counts = 0;
                found_min_val = max_val;

#ifdef HRA_TEST
                if(detected % 5 == 0) {
                    const size_t sample_num = p-(begin+hz);
                    printf("BPM %.0f\n", floorf(60.0/((float)sample_num/hz)*detected));
                }
#endif
            }

            continue;
        }

        if(found_min_counts == 0) {
            found_min_val = sample;
        }

        if(sample == found_min_val) {
            found_min_counts++;
        }
    }

#ifdef HRA_TEST
    const size_t sample_num = end-(begin+hz);
    printf("FOUND Peaks: final=%u, duration=%.01f\n", detected, (float)sample_num/hz);
#endif
    
    return detected;
}
