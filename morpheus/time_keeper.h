#pragma once
#include <stdint.h>

/**
 * keeps track of rtc from mid board
 */
void time_keeper_init(void);

void time_keeper_set(uint32_t unix_time);

uint32_t time_keeper_get(void);
