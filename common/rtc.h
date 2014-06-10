// vi:noet:sw=4 ts=4

#pragma once

#include <stdbool.h>
#include <stdint.h>

enum rtc_weekday_t {
    //< Page 11.
    RTC_SUNDAY = 0b1,
    RTC_MONDAY = 0b10,
    RTC_TUESDAY = 0b11,
    RTC_WEDNESDAY = 0b100,
    RTC_THURSDAY = 0b101,
    RTC_FRIDAY = 0b110,
    RTC_SATURDAY = 0b111,
};

enum rtc_month_t {
    //< Page 11.
    RTC_JANUARY = 0b1,
    RTC_FEBRUARY = 0b10,
    RTC_MARCH = 0b11,
    RTC_APRIL = 0b100,
    RTC_MAY = 0b101,
    RTC_JUN = 0b110,
    RTC_JULY = 0b111,
    RTC_AUGUST = 0b1000,
    RTC_SEPTEMBER = 0b1001,
    RTC_OCTOBER = 0b10000,
    RTC_NOVEMBER = 0b10001,
    RTC_DECEMBER = 0b10010,
};

enum rtc_century_t {
    //< Page 11.
    RTC_CENTURY_21ST = 0b0, // 2000-2099 (note: 2000 = leap year)
    RTC_CENTURY_22ND = 0b1, // 2100-2199
    RTC_CENTURY_23RD = 0b10, // 2200-2299
    RTC_CENTURY_24TH = 0b11, // 2300-2399
};

struct rtc_time_t
{
    // This struct is designed to exactly mirror the on-the-wire data
    // format that the hardware sends back to us if we read registers
    // 0x00...0x07 over I2C.  This will only work on a _little-endian_
    // system (such as the nRF51822 and nRF51422), since the
    // bit-fields are in LSB to MSB order. See page 9 of the
    // "Application Manual AB-RTCMC-32.768kHz-RTC-S7" PDF for the
    // field layout.

    union {
        struct {
            // All of these fields are in BCD (Binary-Coded
            // Decimal). Use the rtc_bcd_decode() function to decode
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
            enum rtc_weekday_t weekday:4; // BCD (0 = Sunday, 6 = Saturday)
            uint8_t date:6; // BCD, Date of month, i.e. 0-31
            uint8_t _padding2:2;

            enum rtc_month_t month:5; // BCD, 1-12
            uint8_t _padding3:1;
            enum rtc_century_t century:2;
            uint8_t year; // BCD, 0-99
        } __attribute__((packed));

        uint8_t bytes[8];
    };
} __attribute__((packed));

void rtc_read(struct rtc_time_t* time);

static inline uint8_t
rtc_bcd_decode(uint8_t value)
{
    return ((value >> 7) & 1) * 80
        + ((value >> 6) & 1) * 40
        + ((value >> 5) & 1) * 20
        + ((value >> 4) & 1) * 10
        + (value & 0b1111);
}

uint8_t rtc_bcd_encode(uint8_t value, bool* success);

struct hlo_ble_time;
bool rtc_time_from_ble_time(struct hlo_ble_time* ble_time, struct rtc_time_t* out_out_rtc_time);
void rtc_time_to_ble_time(struct rtc_time_t* rtc_time, struct hlo_ble_time* out_ble_time);
