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
			uint8_t weekday; // 0-7; 1-7=Mon-Sun, 0=Unknown
		} __attribute__((packed));
		uint8_t bytes[8];
		uint64_t monotonic_time;
	} __attribute__((packed));
} __attribute__((packed));

#ifdef DEBUG
void hlo_ble_time_printf(struct hlo_ble_time* ble_time);
#else
static inline void hlo_ble_time_printf(struct hlo_ble_time* ble_time)
{
}
#endif
