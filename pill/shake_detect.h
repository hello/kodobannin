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
void ShakeDetectDecWindow(void);
bool ShakeDetect(uint32_t accelmag);
void ShakeDetectFactoryTest(void);//enable hi sensitivity for N shakes

void set_shake_detection_callback(shake_detection_callback_t callback);



