// vi:noet:sw=4 ts=4

#include <twi_master.h>

#include "ab_rtcmc_32768khz_aigz_s7.h"

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
    BOOL_OK(twi_master_transfer(RTC_ADDRESS_READ, time->bytes, sizeof(time->bytes), TWI_ISSUE_STOP));
}
