// vi:noet:sw=4 ts=4

// platform.h (ver 9/25/14) morpheus EVT2 pin assignments

#pragma once
#define NC 32

#define BLE_ENABLE
#define ANT_ENABLE
#define HAS_CC3200
#define MSG_BASE_USE_BIG_POOL

#define PLATFORM_HAS_SERIAL
#define HW_REVISION 3

#define configTOTAL_HEAP_SIZE 1024
#define configLOW_MEM 128

#define DEVICE_ID_SIZE        8

//use hello's ant network key
#define USE_HLO_ANT_NETWORK

enum {
    SERIAL_TX_PIN = 19,
    SERIAL_RX_PIN = 17,
    SERIAL_CTS_PIN = 20, //unconnected
    SERIAL_RTS_PIN = 18, //unconnected
};

// uart serial diagnostic port
// MCU_TXD  P19      // 27 P0.19
// MCU_RXD  P17      // 25 P0.17

// proximity detector interrupt
// PROX_INT P07      // 11 P0.07 // from proximity sensor

#define PLATFORM_HAS_SSPI

enum {
    SSPI_nCS  = 21,
    SSPI_SCLK = 25,
    SSPI_MOSI = 23,
    SSPI_MISO = 22,

    SSPI_INT = 24,
};

// general purpose CCU SPI interface
// GSPI_SSEL P21      // 40 P0.21
// GSPI_SCLK P25      // 44 P0.25
// GSPI_MOSI P23      // 42 P0.23
// GSPI_MISO P22      // 41 P0.22
// GSPI_INT  P24      // 43 P0.24

#define PLATFORM_HAS_RTC

enum {
    RTC_SCL = 11,
    RTC_SDA = 9,

    RTC_INT = 15,

    RTC_CLK = 27,
    RTC_OUT = 26,
};

// local I2C for Real Time Calendar (if present)
// RTC_SCL  P11      // 17 P0.11
// RTC_SDA  P09      // 15 P0.09
// RTC_INT  P15      // 21 P0.15 // from real time calendar
// XTL_CLK  P27      // 46 P0.27 AIN1 // low frequency crystal input
// XTL_OUT  P26      // 45 P0.26 AIN0 // low frequency crystal output

#define PLATFORM_HAS_I2C

enum {
    I2C_SCL = 29,
    I2C_SDA = 28,
};

// sensor I2C interface
// I2C_SCL  P29      // 48 P0.29
// I2C_SDA  P28      // 47 P0.28

//#define PLATFORM_HAS_USB_ADC

enum {
    USB_ADC_DM = 4,
    USB_ADC_DP = 1,
};

// ADC / GPIO USB
// USB_DM   P04      //  8 P0.04 AIN5
// USB_DP   P01      //  5 P0.01 AIN2

#define PLATFORM_HAS_SERIAL_CROSS_CONNECT

enum {
    CCU_TX_PIN = 4,
    CCU_RX_PIN = 1,
    CCU_CTS_PIN = NC,
    CCU_RTS_PIN = NC,
};

// cross connect UART serial port
// CCU_TXD  P20      // 28 P0.20
// CCU_RXD  P18      // 26 P0.18

#define PLATFORM_HAS_PMIC_EN

enum {
    PMIC_EN_3V3  = 0,
    PMIC_EN_1V8  = 30,
};

// pmic power supply enable/disable
// EN_3V3   P00      //  4 P0.00
// EN_1V8   P30      //  3 P0.30

#define PLATFORM_HAS_MONITOR

enum {
    VBUS_SENSE  = 6,

    PMIC_3V3_SENSE  = 5,
    PMIC_1V8_SENSE  = 3,

    VMCU_SENSE  = 2,
};

// power supply voltage dividers
// VBUS     P06      // 10 P0.06 AIN7 VBUS 3.2 523/215
// P3V3     P05      //  9 P0.05 AIN6 VDD  3.3 523/215
// P1V8     P03      //  7 P0.03 AIN4 VDD  1.8 523/523

// version voltage divider
// VMCU     P02      //  6 P0.02 AIN3 VMCU 523/523 Divider

#define PLATFORM_HAS_FSPI

enum {
    FSPI_RST  = 16,
    FSPI_WPT  = 12,

    FSPI_SSEL =  8,
    FSPI_SCLK = 14,
    FSPI_MOSI = 13,
    FSPI_MISO = 10,
};

// FRAM reset and write protect control
// FRAM_RST P16      // 22 P0.16
// FRAM_WPT P12      // 18 P0.12

// local FRAM SPI interface (Vdd 2.0V min)
// FRAM_SSEL P08      // 14 P0.08
// FRAM_SCLK P14      // 20 P0.14
// FRAM_MOSI P13      // 19 P0.13
// FRAM_MISO P10      // 16 P0.10
