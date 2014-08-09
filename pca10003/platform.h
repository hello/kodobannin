// vi:noet:sw=4 ts=4

#pragma once

#include <stdint.h>

#include <nrf51.h>
#include <nrf51_bitfields.h>

#define PLATFORM_HAS_SERIAL
enum {
    // serial (UART)
    SERIAL_CTS_PIN = 0,
    SERIAL_RTS_PIN = 0,
    SERIAL_RX_PIN = 6,
    SERIAL_TX_PIN = 4,
    // LEDs
    GPIO_HRS_PWM_G1 = 18,
    GPIO_HRS_PWM_G2 = 19,
};
