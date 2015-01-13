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

    nrf_gpio_cfg_output(GPIO_HRS_PWM_G1);

    while(1) {
        nrf_gpio_pin_set(GPIO_HRS_PWM_G1);
        nrf_delay_ms(500);
        nrf_gpio_pin_clear(GPIO_HRS_PWM_G1);
        nrf_delay_ms(500);
    }
}
