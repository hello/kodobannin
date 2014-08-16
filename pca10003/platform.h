// vi:noet:sw=4 ts=4

#pragma once

#include <stdint.h>

#include <nrf51.h>
#include <nrf51_bitfields.h>

//#define ANT_ENABLE
//#define BLE_ENABLE

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

enum {
    IMU_SPI_MOSI = 12,
    IMU_SPI_MISO = 10,
    IMU_SPI_SCLK = 14,
    IMU_SPI_nCS = 16,
    IMU_INT = 8,
};
