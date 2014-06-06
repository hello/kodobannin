// vi:noet:sw=4 ts=4

#include <nrf_delay.h>
#include <nrf_gpio.h>

#include "platform.h"
#include "util.h"

void
_start()
{
    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);
    PRINTS("Hello, world!\n");
}
