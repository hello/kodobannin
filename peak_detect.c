//
//  HEHeartRateAnalysis.cpp
//  Senkusha
//
//  Created by André Pang on 9/24/13.
//  Copyright (c) 2013 Hello Inc. All rights reserved.
//

#include <stdlib.h>

#ifdef HEART_RATE_DEBUG
#include <stdio.h>
#endif

#include "peak_detect.h"

// See <http://stackoverflow.com/questions/3437404/min-and-max-in-c/3437484#3437484> for these macros.
#define MAX(a,b) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a > _b ? _a : _b; })

#define MIN(a,b) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a > _b ? _b : _a; })

extern void* memcpy(void *s1, const void *s2, size_t n);
extern void* memset(void *b, int c, size_t len);

/// Returns the sum and maximum value in the array for the pointer range [begin, end]. Either out_sum or out_max can be NULL, in which case that result is thrown away (but the processing is still performed).
static void summax(const sample_t* begin, const sample_t* end, sample_t* out_sum, sample_t* out_max)
{
    sample_t sum = *begin;
    sample_t max = *begin;
    const sample_t* p;

    for(p = begin+1; p < end; p++) {
	sum += *p;
	if(*p > max) {
	    max = *p;
	}
    }

    if(out_sum) {
	*out_sum = sum;
    }

    if(out_max) {
	*out_max = max;
    }
}

static void smoothing(const sample_t* const data, const sample_rate_t data_size, const size_t width, sample_t* results)
{
    sample_t window[width];
    memset(window, 0, sizeof(window));

    size_t results_size = data_size+width;

    size_t i;
    for(i = 0; i < results_size; i++) {
	// size_t ind = i % width;
	size_t ind = i & (width - 1); // same as "i % width", but requires width to be a power of 2

	window[ind] = i < data_size ? data[i] : 0;
	
	sample_t sum;
	summax(window, window+width, &sum, NULL);

	*(results+i) = sum/(sample_t)width;
    }
}

static void pre_analysis(const sample_t* const data, const sample_rate_t hz, const size_t width, threshold_t threshold_percentage, float* out_min_interval, threshold_t* out_threshold)
{
    const size_t smoothed_size = hz+width;
    sample_t smoothed[smoothed_size];

    if(out_min_interval) {
	*out_min_interval = MAX_HEART_RATE_INTERVAL / (1.0f/(float)hz);
    }

    if(!out_threshold) {
	return;
    }

    smoothing(data, hz, width, smoothed);

    sample_t sum, max;
    summax(smoothed, smoothed+smoothed_size, &sum, &max);

    sample_t avg = sum/(sample_t)smoothed_size;

    if(threshold_percentage == 0) {
	if(max > 3) {
            threshold_percentage = -0.139410f * max + 1.793787 - 0.05f;
	} else {
            threshold_percentage = -0.139410f * max + 1.793787 + 0.15f;
	}
        threshold_percentage = MAX(1.0f, threshold_percentage);
    }

    const threshold_t threshold = threshold_percentage * avg;
    *out_threshold = threshold * width;

#ifdef HEART_RATE_DEBUG
    printf("Max %.6f, avg %.6f, Thres %.6f (%.6f), min_int %.6f\n", max, avg, threshold, threshold_percentage, *out_min_interval);
#endif
}

/// Use 0.0f for the threshold_percentage unless you know what you're doing.
sample_t detect_peaks(sample_t* samples, size_t samples_size, const sample_rate_t hz, const threshold_t threshold_percentage) {
    static const size_t width = 8;

    const sample_t* const pre_samples = samples;
    const size_t pre_samples_size = hz;

    float min_interval;
    threshold_t threshold;
    pre_analysis(pre_samples, hz, width, threshold_percentage, &min_interval, &threshold);

    sample_t* values = samples + (size_t)hz;
    size_t values_size = samples_size - hz;

    const size_t data_size = width;
    sample_t data[data_size];
    memcpy(data, pre_samples+pre_samples_size-width, sizeof(sample_t)*width);

#ifdef HEART_RATE_DEBUG
    {
	size_t i;
	for(i = 0; i < width; i++) {
	    printf("%f\n", data[i]);
	}
    }
#endif

    size_t sample_num = 0;

    sample_t current_avg, prev_avg, prev_prev_avg;
    summax(data, data+data_size, &current_avg, NULL);
    summax(pre_samples+pre_samples_size-(width+1),
	   pre_samples+pre_samples_size-1, &prev_avg, NULL);
    summax(pre_samples+pre_samples_size-(width+2),
	   pre_samples+pre_samples_size-2, &prev_prev_avg, NULL);

#ifdef HEART_RATE_DEBUG
    printf("avg: %f, prev: %f, prevprev: %f\n", current_avg, prev_avg, prev_prev_avg);
#endif

    sample_t prev_peak_sample_num = -min_interval;

#ifdef HEART_RATE_DEBUG
    unsigned raw_detected = 0;
#endif
    unsigned detected = 0;

    size_t i;
    for(i = 0; i < values_size; i++) {
	sample_t sample = values[i];

	// size_t cycle = sample_num % width;
	size_t cycle = sample_num & (width - 1); // same as "i % width", but requires width to be a power of 2

	prev_prev_avg = prev_avg;
	prev_avg = current_avg;

	current_avg -= data[cycle];
	data[cycle] = sample;
	current_avg += sample;

#ifdef HEART_RATE_DEBUG
	printf("pp: %f, p: %f, a: %f, d[c]: %f, t: %f\n", prev_prev_avg, prev_avg, current_avg, data[cycle], threshold);
#endif

	if(prev_avg > threshold && prev_prev_avg < prev_avg && prev_avg > current_avg) {
#ifdef HEART_RATE_DEBUG
	    printf("raw peak detected, sample %lu, val %f %f %f\n", sample_num+hz, prev_prev_avg, prev_avg, current_avg);
	    raw_detected++;
#endif
	    if((sample_num - prev_peak_sample_num) >= min_interval) {
		prev_peak_sample_num = sample_num;

		detected++;
#ifdef HEART_RATE_DEBUG
		if (detected % 5 == 0) {
		    printf("BPM %.1f\n", 60.0f/(sample_num*hz)*detected);
		}
#endif
	    }
	}

	sample_num++;
    }

    float duration = sample_num/hz;
#ifdef HEART_RATE_DEBUG
    printf("Peaks: raw=%u final=%u duration=%0.1f\n", raw_detected, detected, duration);
#endif

    return (sample_rate_t)(60.0f/duration*detected);
}
