// vi:noet:sw=4 ts=4

#pragma once

#include <stdint.h>

struct hlo_ble_time
{
    union {
        struct {
            uint16_t year;
            uint8_t month;
            uint8_t day;
            uint8_t hours;
            uint8_t minutes;
            uint8_t seconds;
        };
        uint8_t bytes[7];
    };
} __attribute__((packed));
