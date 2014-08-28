#pragma once
#include <stdint.h>
#include <stdbool.h>
/**
 * Detects if a shake has occured, give time period.
 * 
 * Simple algorithm that counts #of shakes above threshold
 */

void ShakeDetectReset(uint32_t threshold, uint16_t count);
void ShakeDetectDecWindow(void);
bool ShakeDetect(uint32_t accelmag);



