// vi:noet:sw=4 ts=4

#pragma once

#include "util.h"

enum aigz_weekday_t {
    //< Page 11.
    AIGZ_SUNDAY = 0b1,
    AIGZ_MONDAY = 0b10,
    AIGZ_TUESDAY = 0b11,
    AIGZ_WEDNESDAY = 0b100,
    AIGZ_THURSDAY = 0b101,
    AIGZ_FRIDAY = 0b110,
    AIGZ_SATURDAY = 0b111,
};

enum aigz_month_t {
    //< Page 11.
    AIGZ_JANUARY = 0b1,
    AIGZ_FEBRUARY = 0b10,
    AIGZ_MARCH = 0b11,
    AIGZ_APRIL = 0b100,
    AIGZ_MAY = 0b101,
    AIGZ_JUN = 0b110,
    AIGZ_JULY = 0b111,
    AIGZ_AUGUST = 0b1000,
    AIGZ_SEPTEMBER = 0b1001,
    AIGZ_OCTOBER = 0b10000,
    AIGZ_NOVEMBER = 0b10001,
    AIGZ_DECEMBER = 0b10010,
};

enum aigz_century_t {
    //< Page 11.
    AIGZ_CENTURY_21ST = 0b0, // 2000-2099 (note: 2000 = leap year)
    AIGZ_CENTURY_22ND = 0b1, // 2100-2199
    AIGZ_CENTURY_23RD = 0b10, // 2200-2299
    AIGZ_CENTURY_24TH = 0b11, // 2300-2399
};

struct aigz_time_t
{
    // This struct is designed to exactly mirror the on-the-wire data
    // format that the hardware sends back to us if we read registers
    // 0x00...0x07 over I2C.  This will only work on a _little-endian_
    // system (such as the nRF51822 and nRF51422), since the
    // bit-fields are in LSB to MSB order. See page 9 of the
    // "Application Manual AB-RTCMC-32.768kHz-AIGZ-S7" PDF for the
    // field layout.

    union {
        struct {
            // All of these fields are in BCD (Binary-Coded
            // Decimal). Use the aigz_bcd_decode() function to decode
            // them.

            uint8_t hundredths:4; // BCD, 0-9
            uint8_t tenths:4; // BCD, 0-9
            uint8_t seconds:7; // BCD, 0-59
            uint8_t oscillator_enabled:1; // 32.768KHz oscillator enabled (page 10)

            uint8_t minutes:7; // BCD, 0-59
            uint8_t oscillator_fail_interrupt_enabled:1; // (page 10)
            uint8_t hours:6; // BCD, 0-23
            uint8_t _padding1:2;

            uint8_t clockout_frequency:4; // (page 10)
            enum aigz_weekday_t weekday:4; // BCD (0 = Sunday, 6 = Saturday)
            uint8_t date:6; // BCD, Date of month, i.e. 0-31
            uint8_t _padding2:2;

            enum aigz_month_t month:5; // BCD, 1-12
            uint8_t _padding3:1;
            enum aigz_century_t century:2;
            uint8_t year; // BCD, 0-99
        } PACKED;

        uint8_t bytes[8];
    };
} PACKED;

void aigz_read(struct aigz_time_t* time);

static inline uint8_t
aigz_bcd_decode(uint8_t value)
{
    return ((value >> 7) & 1) * 80
        + ((value >> 6) & 1) * 40
        + ((value >> 5) & 1) * 20
        + ((value >> 4) & 1) * 10
        + (value & 0b1111);
}
