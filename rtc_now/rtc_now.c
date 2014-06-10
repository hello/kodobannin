#include <stdio.h>
#include <time.h>

#include "rtc.h"

uint8_t
rtc_bcd_encode(uint8_t value, bool* success)
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
    .year = now->tm_year%100,
  };

  printf("Now: %02X %02X %02X %02X %02X %02X %02X\n",
	 rtc_time.bytes[0], rtc_time.bytes[1], rtc_time.bytes[2], rtc_time.bytes[3], rtc_time.bytes[4], rtc_time.bytes[5], rtc_time.bytes[6]);
}
