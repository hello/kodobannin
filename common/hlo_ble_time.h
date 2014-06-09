// vi:noet:sw=4 ts=4

#pragma once

#include <stdint.h>

struct hlo_ble_time
{
    union {
        struct {
            uint16_t year; // 1582-9999
            uint8_t month; // 1-12, or 0 (unknown)
            uint8_t day; // 1-31, or 0 (unknown)
            uint8_t hours; // 0-23
            uint8_t minutes; // 0-59
            uint8_t seconds; // 0-59
        };
        uint8_t bytes[7];
    };
} __attribute__((packed));
