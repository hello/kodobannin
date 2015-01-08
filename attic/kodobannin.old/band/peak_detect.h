//
//  HEHeartRateAnalysis.h
//  Senkusha
//
//  Created by André Pang on 9/24/13.
//  Copyright (c) 2013 Hello Inc. All rights reserved.
//

#ifndef PEAK_DETECT_H

#define PEAK_DETECT_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t sample_t;
typedef uint8_t sample_rate_t;
typedef uint8_t peaks_t; // (60.0f / ((sample_count-hz)/hz) * peaks) to get actual BPM

void pre_analysis(const sample_t* const begin, const sample_t* const end, sample_t* out_min_value, sample_t* out_max_value);
peaks_t detect_peaks(const sample_t* const begin, const sample_t* const end, const sample_rate_t hz);

#endif // PEAK_DETECT_H
