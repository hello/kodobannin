// vi:noet:sw=4 ts=4

#include <stdio.h>
#include <time.h>

#include "hlo_ble_time.h"
#include "rtc.h"

bool
rtc_time_from_ble_time(struct hlo_ble_time* ble_time, struct rtc_time_t* out_rtc_time)
{
    if(ble_time->day == 0 || ble_time->month == 0) {
        // Cannot set to unknown day/month
        return false;
    }

    *out_rtc_time = (struct rtc_time_t){};

    out_rtc_time->hundredths = 0;
    out_rtc_time->tenths = rtc_bcd_encode(0, NULL);
    out_rtc_time->seconds = rtc_bcd_encode(ble_time->seconds, NULL);
    out_rtc_time->minutes = rtc_bcd_encode(ble_time->minutes, NULL);
    out_rtc_time->hours = rtc_bcd_encode(ble_time->hours, NULL);

    // out_rtc_time->weekday = ?;

    out_rtc_time->date = rtc_bcd_encode(ble_time->day, NULL);
    out_rtc_time->month = rtc_bcd_encode(ble_time->month, NULL);

    if(ble_time->year >= 2000 && ble_time->year <= 2099) {
        out_rtc_time->century = RTC_CENTURY_21ST;
    } else if(ble_time->year >= 2100 && ble_time->year <= 2199) {
        out_rtc_time->century = RTC_CENTURY_22ND;
    } else if(ble_time->year >= 2200 && ble_time->year <= 2299) {
        out_rtc_time->century = RTC_CENTURY_23RD;
    } else if(ble_time->year >= 2300 && ble_time->year <= 2399) {
        out_rtc_time->century = RTC_CENTURY_24TH;
    }

    // Yes, I'm sure this could be done faster; e.g. see <http://electronics.stackexchange.com/questions/12618/fastest-way-to-get-integer-mod-10-and-integer-divide-10>. Considering that this function is called rarely, I'm not too concerned.
    out_rtc_time->year = rtc_bcd_encode(ble_time->year % 100, NULL);

    return true;
}

void
rtc_time_to_ble_time(struct rtc_time_t* rtc_time, struct hlo_ble_time* out_ble_time)
{
    switch(rtc_time->century) {
    case RTC_CENTURY_21ST:
        out_ble_time->year = 2000;
        break;
    case RTC_CENTURY_22ND:
        out_ble_time->year = 2100;
        break;
    case RTC_CENTURY_23RD:
        out_ble_time->year = 2200;
        break;
    case RTC_CENTURY_24TH:
        out_ble_time->year = 2300;
        break;
    }
    out_ble_time->year += rtc_bcd_decode(rtc_time->year);

    out_ble_time->month = rtc_bcd_decode(rtc_time->month);
    out_ble_time->day = rtc_bcd_decode(rtc_time->date);
    out_ble_time->hours = rtc_bcd_decode(rtc_time->hours);
    out_ble_time->minutes = rtc_bcd_decode(rtc_time->minutes);
    out_ble_time->seconds = rtc_bcd_decode(rtc_time->seconds);
}

uint8_t
rtc_bcd_encode(uint8_t value, bool* success)
{
    enum {
        RTC_BCD_MAX = 80 + 40 + 20 + 10 + 8 + 4 + 2 + 1,
    };

    if(value > RTC_BCD_MAX) {
        if(success) {
            *success = false;
        }
        return UINT8_MAX;
    }

    uint8_t encoded = 0;

    while(value) {
        if(value >= 80) {
            encoded |= 1 << 7;
            value -= 80;
        } else if(value >= 40) {
            encoded |= 1 << 6;
            value -= 40;
        } else if(value >= 20) {
            encoded |= 1 << 5;
            value -= 20;
        } else if(value >= 10) {
            encoded |= 1 << 4;
            value -= 10;
        } else {
            encoded |= value;
            break;
        }
    }

    if(success) {
        *success = true;
    }

    return encoded;
}

int main(int argc, char** argv)
{
    time_t seconds_since_epoch = time(NULL);
    printf("seconds: %ld\n", seconds_since_epoch);

    struct tm* now = localtime(&seconds_since_epoch);
    char* human_readable_now = ctime(&seconds_since_epoch);
    printf("year: %d\n", now->tm_year%100);
    printf("human: %s\n", human_readable_now);

    struct rtc_time_t rtc_time = {
        .hundredths = 0,
        .tenths = 0,
        .seconds = rtc_bcd_encode(now->tm_sec, NULL),
        .oscillator_enabled = 0,

        .minutes = rtc_bcd_encode(now->tm_min, NULL),
        .oscillator_fail_interrupt_enabled = 0,
        .hours = rtc_bcd_encode(now->tm_hour, NULL),
        ._padding1 = 0,

        .clockout_frequency = 0,
        .weekday = rtc_bcd_encode(now->tm_wday, NULL),
        .date = rtc_bcd_encode(now->tm_mday, NULL),
        ._padding2 = 0,

        .month = rtc_bcd_encode(now->tm_mon+1, NULL),
        ._padding3 = 0,
        .century = (now->tm_year/100)-1,
        .year = rtc_bcd_encode(now->tm_year%100, NULL),
    };

    printf("RTC: %02X %02X %02X %02X %02X %02X %02X %02X\n",
           rtc_time.bytes[0], rtc_time.bytes[1], rtc_time.bytes[2], rtc_time.bytes[3], rtc_time.bytes[4], rtc_time.bytes[5], rtc_time.bytes[6], rtc_time.bytes[7]);

    struct hlo_ble_time ble_time;
    rtc_time_to_ble_time(&rtc_time, &ble_time);
    printf("BLE: %02X %02X %02X %02X %02X %02X %02X\n",
           ble_time.bytes[0], ble_time.bytes[1], ble_time.bytes[2], ble_time.bytes[3], ble_time.bytes[4], ble_time.bytes[5], ble_time.bytes[6]);

    rtc_time_from_ble_time(&ble_time, &rtc_time);
    printf("RTC: %02X %02X %02X %02X %02X %02X %02X %02X\n",
           rtc_time.bytes[0], rtc_time.bytes[1], rtc_time.bytes[2], rtc_time.bytes[3], rtc_time.bytes[4], rtc_time.bytes[5], rtc_time.bytes[6], rtc_time.bytes[7]);
}
