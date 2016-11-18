// vi:noet:sw=4 ts=4

// platform.h (ver 9/25/14) morpheus EVT2 pin assignments

#pragma once
#define NC 32

#define BLE_ENABLE
#define ANT_ENABLE
#define HAS_CC3200

#define PLATFORM_HAS_SERIAL
#define HW_REVISION 3

#define DEVICE_ID_SIZE        8


#define configTOTAL_HEAP_SIZE 1152
#define configLOW_MEM 128

//use hello's ant network key
#define USE_HLO_ANT_NETWORK

enum {
    SERIAL_TX_PIN = 19,
    SERIAL_RX_PIN = 17,
    SERIAL_CTS_PIN = 20, //unconnected
    SERIAL_RTS_PIN = 18, //unconnected
};

#define PLATFORM_HAS_SSPI

enum {
    SSPI_nCS  = 21,
    SSPI_SCLK = 25,
    SSPI_MOSI = 23,
    SSPI_MISO = 22,

    SSPI_INT = 24,
};
/*  Removed for MP
#define PLATFORM_HAS_ACCEL_SPI

enum{
	ACCEL_nCS = 28,
	ACCEL_SCLK = 8,
	ACCEL_MOSI = 13,
	ACCEL_MISO = 10,
	ACCEL_INT = 29,
};
*/
#define PLATFORM_HAS_SERIAL_CROSS_CONNECT

enum {
    CCU_TX_PIN = 1,
    CCU_RX_PIN = 4,
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
