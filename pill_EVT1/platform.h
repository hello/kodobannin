// vi:noet:sw=4 ts=4

#pragma once

#ifdef ANT_STACK_SUPPORT_REQD
//#define ANT_ENABLE
#endif

#define BLE_ENABLE
//#define BATTERY_SERVICE_ENABLE
//#define DEBUG_BATT_LVL

#define PLATFORM_HAS_IMU

enum {
    IMU_SPI_MOSI = 12,
    IMU_SPI_MISO = 10,
    IMU_SPI_SCLK = 14,
    IMU_SPI_nCS = 16,
    IMU_INT = 8,
};

#define PLATFORM_HAS_RTC
enum {
    RTC_SDA = 0,
    RTC_SCL = 24,
    RTC_INT = 28,
};

#define PLATFORM_HAS_GAUGE
enum {
    GAUGE_SDA = 0,
    GAUGE_SCL = 25,
    GAUGE_INT = 21,
};

#define PLATFORM_HAS_SERIAL

enum {
    SERIAL_TX_PIN = 20,
    SERIAL_RX_PIN = 18,
    SERIAL_CTS_PIN = 0,
    SERIAL_RTS_PIN = 0,
};

