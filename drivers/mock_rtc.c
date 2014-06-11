// vi:noet:sw=4 ts=4

#include <string.h>

#include <twi_master.h>

#include "rtc.h"
#include "util.h"

static struct rtc_time_t _mock_time;

void
rtc_read(struct rtc_time_t* rtc_time)
{
    *rtc_time = _mock_time;
}

void
rtc_write(struct rtc_time_t* rtc_time)
{
    _mock_time = *rtc_time;
}
