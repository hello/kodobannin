// vi:noet:sw=4 ts=4

#include <twi_master.h>

#include "rtc.h"
#include "util.h"

enum {
    RTC_ADDRESS_WRITE = (0b1101000 << 1),
    RTC_ADDRESS_READ = (0b1101000 << 1) | TWI_READ_BIT,
};

void
rtc_read(struct rtc_time_t* time)
{
    uint8_t RTC_CLOCK_SECTION_ADDRESS = 0x0;

    BOOL_OK(twi_master_transfer(RTC_ADDRESS_WRITE, &RTC_CLOCK_SECTION_ADDRESS, sizeof(RTC_CLOCK_SECTION_ADDRESS), TWI_ISSUE_STOP));
    BOOL_OK(twi_master_transfer(RTC_ADDRESS_READ, time->bytes, sizeof(time->bytes), TWI_ISSUE_STOP));
}

void
rtc_write(struct rtc_time_t* time)
{
    uint8_t RTC_CLOCK_SECTION_ADDRESS = 0x0;

    BOOL_OK(twi_master_transfer(RTC_ADDRESS_WRITE, &RTC_CLOCK_SECTION_ADDRESS, sizeof(RTC_CLOCK_SECTION_ADDRESS), TWI_ISSUE_STOP));
    BOOL_OK(twi_master_transfer(RTC_ADDRESS_WRITE, time->bytes, sizeof(time->bytes), TWI_ISSUE_STOP));
}
