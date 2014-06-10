// vi:noet:sw=4 ts=4

#include <stddef.h>

#include <twi_master.h>

#include "rtc.h"
#include "hlo_ble_time.h"
#include "util.h"

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

bool
rtc_time_from_ble_time(struct hlo_ble_time* ble_time, struct rtc_time_t* out_rtc_time)
{
    if(ble_time->day == 0 || ble_time->month == 0) {
        // Cannot set to unknown day/month
        DEBUG("Bad incoming time: ", ble_time->bytes);
        return false;
    }

    rtc_read(out_rtc_time);

    out_rtc_time->hundredths = 0;
    out_rtc_time->tenths = rtc_bcd_encode(0);
    out_rtc_time->seconds = rtc_bcd_encode(ble_time->seconds);
    out_rtc_time->minutes = rtc_bcd_encode(ble_time->minutes);
    out_rtc_time->hours = rtc_bcd_encode(ble_time->hours);

    // out_rtc_time->weekday = ?;

    out_rtc_time->date = ble_time->day;
    out_rtc_time->month = ble_time->month;

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
    out_ble_time->year += rtc_time->year;

    out_ble_time->month = rtc_bcd_decode(rtc_time->month);
    out_ble_time->day = rtc_bcd_decode(rtc_time->date);
    out_ble_time->hours = rtc_bcd_decode(rtc_time->hours);
    out_ble_time->minutes = rtc_bcd_decode(rtc_time->minutes);
    out_ble_time->seconds = rtc_bcd_decode(rtc_time->seconds);
}

void rtc_printf(struct rtc_time_t* rtc_time)
{
    printf("%d/%d/%d%d %d:%d:%d.%d%d\r\n",
           rtc_bcd_decode(rtc_time->month),
           rtc_bcd_decode(rtc_time->date),
           20+rtc_time->century, rtc_bcd_decode(rtc_time->year),
           rtc_bcd_decode(rtc_time->hours),
           rtc_bcd_decode(rtc_time->minutes),
           rtc_bcd_decode(rtc_time->seconds),
           rtc_bcd_decode(rtc_time->tenths),
           rtc_bcd_decode(rtc_time->hundredths));
}
