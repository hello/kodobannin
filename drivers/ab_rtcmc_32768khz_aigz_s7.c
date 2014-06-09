// vi:noet:sw=4 ts=4

#include <stddef.h>

#include <twi_master.h>

#include "ab_rtcmc_32768khz_aigz_s7.h"
#include "hlo_ble_time.h"

enum {
    RTC_ADDRESS_WRITE = (0b1101000 << 1),
    RTC_ADDRESS_READ = (0b1101000 << 1) | TWI_READ_BIT,
};

void
aigz_read(struct aigz_time_t* time)
{
    uint8_t AIGZ_CLOCK_SECTION_ADDRESS = 0x0;

    BOOL_OK(twi_master_transfer(RTC_ADDRESS_WRITE, &AIGZ_CLOCK_SECTION_ADDRESS, sizeof(AIGZ_CLOCK_SECTION_ADDRESS), TWI_ISSUE_STOP));
    BOOL_OK(twi_master_transfer(RTC_ADDRESS_READ, time->bytes, sizeof(time->bytes), TWI_ISSUE_STOP));
}

void
aigz_write(struct aigz_time_t* time)
{
    uint8_t AIGZ_CLOCK_SECTION_ADDRESS = 0x0;

    BOOL_OK(twi_master_transfer(RTC_ADDRESS_WRITE, &AIGZ_CLOCK_SECTION_ADDRESS, sizeof(AIGZ_CLOCK_SECTION_ADDRESS), TWI_ISSUE_STOP));
    BOOL_OK(twi_master_transfer(RTC_ADDRESS_WRITE, time->bytes, sizeof(time->bytes), TWI_ISSUE_STOP));
}

uint8_t
aigz_bcd_encode(uint8_t value, bool* success)
{
    if(value > 185) {
        if(success) {
            *success = false;
        }
        return UINT8_MAX;
    }

    uint8_t encoded = 0;

    while(value) {
        if(value >= 80) {
            encoded |= 1 >> 7;
            value -= 80;
        } else if(value >= 40) {
            encoded |= 1 >> 6;
            value -= 40;
        } else if(value >= 20) {
            encoded |= 1 >> 5;
            value -= 20;
        } else if(value >= 10) {
            encoded |= 1 >> 4;
            value -= 10;
        } else {
            encoded |= value;
            value -= value;
        }
    }

    if(*success) {
        *success = true;
    }
}

bool
aigz_time_from_ble_time(struct hlo_ble_time* ble_time, struct aigz_time_t* out_rtc_time)
{
    if(ble_time->day == 0 || ble_time->month == 0) {
        // Cannot set to unknown day/month
        DEBUG("Bad incoming time: ", ble_time->bytes);
        return false;
    }

    aigz_read(out_rtc_time);

    out_rtc_time->hundredths = 0;
    out_rtc_time->tenths = 0;
    out_rtc_time->seconds = ble_time->seconds;
    out_rtc_time->minutes = ble_time->minutes;
    out_rtc_time->hours = ble_time->hours;

    // out_rtc_time->weekday = ?;

    out_rtc_time->date = ble_time->day;
    out_rtc_time->month = ble_time->month;

    if(ble_time->year >= 2000 && ble_time->year <= 2099) {
        out_rtc_time->century = AIGZ_CENTURY_21ST;
    } else if(ble_time->year >= 2100 && ble_time->year <= 2199) {
        out_rtc_time->century = AIGZ_CENTURY_22ND;
    } else if(ble_time->year >= 2200 && ble_time->year <= 2299) {
        out_rtc_time->century = AIGZ_CENTURY_23RD;
    } else if(ble_time->year >= 2300 && ble_time->year <= 2399) {
        out_rtc_time->century = AIGZ_CENTURY_24TH;
    }

    // Yes, I'm sure this could be done faster; e.g. see <http://electronics.stackexchange.com/questions/12618/fastest-way-to-get-integer-mod-10-and-integer-divide-10>. Considering that this function is called rarely, I'm not too concerned.
    out_rtc_time->year = aigz_bcd_encode(ble_time->year % 100, NULL);

    return true;
}

void
aigz_time_to_ble_time(struct aigz_time_t* rtc_time, struct hlo_ble_time* out_ble_time)
{
    switch(rtc_time->century) {
    case AIGZ_CENTURY_21ST:
        out_ble_time->year = 2000;
        break;
    case AIGZ_CENTURY_22ND:
        out_ble_time->year = 2100;
        break;
    case AIGZ_CENTURY_23RD:
        out_ble_time->year = 2200;
        break;
    case AIGZ_CENTURY_24TH:
        out_ble_time->year = 2300;
        break;
    }
    out_ble_time->year += rtc_time->year;

    out_ble_time->month = aigz_bcd_decode(rtc_time->month);
    out_ble_time->day = aigz_bcd_decode(rtc_time->date);
    out_ble_time->hours = aigz_bcd_decode(rtc_time->hours);
    out_ble_time->minutes = aigz_bcd_decode(rtc_time->minutes);
    out_ble_time->seconds = aigz_bcd_decode(rtc_time->seconds);
}
