// vi:noet:sw=4 ts=4

/*
 *  rtc_test (w/cli)
 */

#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <nrf_sdm.h>
#include <softdevice_handler.h>
#include <twi_master.h>

#include "rtc.h"
#include "platform.h"
#include "util.h"

#if DEBUG_SERIAL == 2
  #include "uart.h"
  #include "ucli.h"
#endif

void
_start()
{
 // simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);

    BOOL_OK(twi_master_init());

    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_SYNTH_250_PPM, false);

#if DEBUG_SERIAL == 2
    uartInit();
    ucliInit();
#endif

    struct rtc_time_t time;

    for(;;) { // can now use cli 't' to display current time

     /* rtc_read(&time);
        PRINT_HEX(time.bytes, sizeof(time.bytes));
        PRINTS("| ");

        printf("%d/%d/%d%d %d:%d:%d.%d%d\r\n",
               rtc_bcd_decode(time.month),
               rtc_bcd_decode(time.date),
               20+time.century, rtc_bcd_decode(time.year),
               rtc_bcd_decode(time.hours),
               rtc_bcd_decode(time.minutes),
               rtc_bcd_decode(time.seconds),
               rtc_bcd_decode(time.tenths),
               rtc_bcd_decode(time.hundredths)); */

        nrf_delay_ms(20);

     // APP_OK(SDK_POWER_SYSTEM_OFF());

    }
}

