// vi:noet:sw=4 ts=4

#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <nrf_sdm.h>
#include <twi_master.h>

#include "platform.h"
#include "util.h"

void
_start()
{
    (void) sd_softdevice_disable();

    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);
    PRINTS("i2c test started\n");

    bool twi_inited = twi_master_init();
    if(!twi_inited) {
        PRINTS("twi_master_init() failed\n");
    }

    enum {
        RTC_ADDRESS_WRITE = (0b1101000 << 1),
        RTC_ADDRESS_READ = (0b1101000 << 1) | TWI_READ_BIT,
    };

    const uint8_t rtc_milliseconds_address = 0x0;
    uint8_t milliseconds[1];

    for(;;) {
        bool success;

        success = twi_master_transfer(RTC_ADDRESS_WRITE, &rtc_milliseconds_address, sizeof(rtc_milliseconds_address), TWI_ISSUE_STOP);
        if(!success) {
            PRINTS("write failed\n");
        }

        success = twi_master_transfer(RTC_ADDRESS_READ, milliseconds, sizeof(milliseconds), TWI_ISSUE_STOP);
        if(success) {
            DEBUG("Milliseconds: ", milliseconds);
        } else {
            PRINTS("read failed\n");
        }

        nrf_delay_ms(50);
    }
}
