#pragma once
#include <stdint.h>
#include <stdbool.h>
/**
 * Detects if a shake has occured, give time period.
 * 
 * Simple algorithm that counts #of shakes above threshold
 */

// The bettery way is have a gesture detection callback and define enum for guestures
typedef void(*shake_detection_callback_t)(void);  

void ShakeDetectReset(uint32_t threshold);
bool ShakeDetect(uint32_t accelmag,uint32_t time);

void set_shake_detection_callback(shake_detection_callback_t callback);



