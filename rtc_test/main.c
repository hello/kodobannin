// vi:noet:sw=4 ts=4

#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <nrf_sdm.h>
#include <twi_master.h>

#include "aigz.h"
#include "platform.h"
#include "util.h"

void
_start()
{
    (void) sd_softdevice_disable();

    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);

    BOOL_OK(twi_master_init());

    struct aigz_time_t time;

    for(;;) {
        aigz_read(&time);
        PRINT_HEX(time.bytes, sizeof(time.bytes));
        PRINTS("| ");

        printf("%d/%d/%d%d %d:%d:%d.%d%d\r\n",
               aigz_bcd_decode(time.month),
               aigz_bcd_decode(time.date),
               20+time.century, aigz_bcd_decode(time.year),
               aigz_bcd_decode(time.hours),
               aigz_bcd_decode(time.minutes),
               aigz_bcd_decode(time.seconds),
               aigz_bcd_decode(time.tenths),
               aigz_bcd_decode(time.hundredths));

        nrf_delay_ms(20);
    }
}
