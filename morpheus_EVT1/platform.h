// vi:noet:sw=4 ts=4

#pragma once

#define PLATFORM_HAS_SSPI

enum {
    SSPI_MOSI = 15,
    SSPI_MISO = 11,
    SSPI_SCLK = 12,
    SSPI_nCS = 9,
};

#define PLATFORM_HAS_RTC

enum {
    RTC_SCL = 24,
    RTC_SDA = 28,
    RTC_INT = 29,
    RTC_CLK = 27,
};

#define PLATFORM_HAS_SERIAL

enum {
    SERIAL_TX_PIN = 1,
    SERIAL_RX_PIN = 3,
    SERIAL_CTS_PIN = 0,
    SERIAL_RTS_PIN = 0,
};
