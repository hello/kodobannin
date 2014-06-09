// vi:noet:sw=4 ts=4

#include <string.h>

#include <twi_master.h>

#include "rtc.h"
#include "util.h"

void
rtc_read(struct rtc_time_t* time)
{
  memset(time, 0x55, sizeof(*time));
}

void
rtc_write(struct rtc_time_t* time)
{
}
