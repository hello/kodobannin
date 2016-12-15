// vi:noet:sw=4 ts=4

#pragma once

#ifdef ANT_STACK_SUPPORT_REQD
#define ANT_ENABLE
#endif

#define BLE_ENABLE

#define HW_REVISION 4

#define DEVICE_KEY_ADDRESS                   0x20003FF0
//use hello's ant network key
#define USE_HLO_ANT_NETWORK
#define PILL_ANT_TYPE HLO_ANT_DEVICE_TYPE_PILL
#define PILL_HW_TYPE  HLO_ANT_DEVICE_TYPE_PILL1_9

#define configTOTAL_HEAP_SIZE 1024

/*
// This is from EVT1, not used
// TODO: Maybe just usesomething from PLATFORM_HAS_I2C?

#define PLATFORM_HAS_GAUGE
enum {
    GAUGE_SDA = 0,
    GAUGE_SCL = 25,
    GAUGE_INT = 21,
};

*/

#define PLATFORM_HAS_SERIAL

enum {
    SERIAL_TX_PIN = 18, // 18,
    SERIAL_RX_PIN = 17, // 19,
    SERIAL_CTS_PIN = 31,
    SERIAL_RTS_PIN = 31,
};

// uart serial diagnostic port
// MCU_TXD  P18      // 26 P0.18
// MCU_RXD  P19      // 27 P0.19

#define PLATFORM_HAS_IMU
#define PLATFORM_HAS_IMU_VDD_CONTROL

enum {
    IMU_VDD_EN   = 19,

    IMU_SPI_nCS  = 13, // 16,
    IMU_SPI_SCLK = 15, // 14,
    IMU_SPI_MOSI =  9, // 12,
    IMU_SPI_MISO = 11, // 10,

    IMU_INT = 16, // 23,
};

// inertial motion sensor
// IMU_EN   P20      // 28 P0.20
// IMU_SSEL P16      // 22 P0.16
// IMU_SCLK P12      // 18 P0.12
// IMU_MOSI P14      // 20 P0.14
// IMU_MISO P10      // 16 P0.10
// IMU_INT  P23      // 42 P0.23

#define PLATFORM_HAS_I2C
#define PLATFORM_HAS_PROX

enum {
    I2C_SCL = 23, // 21,
    I2C_SDA = 24, // 22,

    RTC_INT =  2,
    LDO_INT = 24,
    PROX_VDD_EN  = 20, // VPROX_VDD_EN
    PROX_BOOST_ENABLE  = 10, // 11, BOOST ENABLE
    PROX_BOOST_MODE  = 22, // 11, BOOST ENABLE
};

// Real Time Calendar
// RTC_SCL  P21      // 40 P0.21
// RTC_SDA  P22      // 41 P0.22
// RTC_INT  P02      //  6 P0.02 AIN3 // from real time calendar
// LDO_INT  P24      // 43 P0.24 // from gas gauge
// XTL_CLK  P27      // 46 P0.27 AIN1 // low frequency crystal input
// XTL_OUT  P26      // 45 P0.26 AIN0 // low frequency crystal output

#define PLATFORM_HAS_BATTERY
#define PLATFORM_HAS_VERSION

enum {
    VBAT_SENSE  = 6,
    VMCU_SENSE  = 2,

    VMCU_REBOOT  = 22,

    VBAT_VER_EN = 0,
};

#define LDO_VBAT_ADC_INPUT    0x80UL
#define LDO_VMCU_ADC_INPUT    0x08UL

// power supply voltage dividers
// VBAT     P06      // 10 P0.06 AIN7 VBUS 3.2 523/215
// version voltage divider
// VMCU     P02      //  6 P0.02 AIN3 VMCU 523/523 Divider
// vbat and version measurement enable
// VER      P00      //  4 P0.00

//#define PLATFORM_HAS_VLED

enum {
    VLED_VDD_EN  = 17, // VPROX_VDD_EN
    VRGB_ENABLE  = 10, // 11, BOOST ENABLE

    VRGB_SELECT  = 21, // 8, BOOST MODE
    VRGB_ADJUST  =  8, // 9,
    VRGB_SENSE   =  4,

    LED3_SENSE   =  3,
    LED2_SENSE   =  1,
    LED1_SENSE   =  5,

    LED3_ENABLE  = 30, // magnetic reed switch (dvt/pvt)
    LED2_ENABLE  = 28, // magnetic reed switch (evt)
    LED1_ENABLE  =  7,
};
#define LED_BLUE_CHANNEL LED1_ENABLE
#define LED_RED_CHANNEL LED3_ENABLE
#define LED_GREEN_CHANNEL LED2_ENABLE

#define LDO_VRGB_ADC_INPUT    0x20UL /* P0.04 AIN5 */

#define LED_RED_ADC_INPUT     0x10UL /* P0.03 AIN4 */
#define LED_GREEN_ADC_INPUT   0x04UL /* P0.01 AIN2 */
#define LED_BLUE_ADC_INPUT    0x40UL /* P0.05 AIN6 */

#define LED_RED_SENSE       LED3_SENSE  /* P0.03 */
#define LED_GREEN_SENSE     LED2_SENSE  /* P0.01 */
#define LED_BLUE_SENSE      LED1_SENSE  /* P0.05 */

#define LED_RED_CHANNEL     LED3_ENABLE /* P0.30 */
#define LED_GREEN_CHANNEL   LED2_ENABLE /* P0.28 */
#define LED_BLUE_CHANNEL    LED1_ENABLE /* P0.07 */

#define PLATFORM_HAS_REED

#define LED_REED_ENABLE     LED3_ENABLE /* P0.30 */

// pFet LED boost power enable
// EN_LED   P17      // 25 P0.17

// BOOST    P11      // 17 P0.11 Boost Converter Enable
// LED_RGB  P09      // 15 P0.09 Red (1.7) or Grn/Blu (2.7)
// LED_ADJ  P08      // 14 P0.08 Adjust RGB Voltage w/PWM RC DAC

// RGB LED Power Supply
// VRGB     P04      //  8 P0.04 AIN5 LED 523/215 Boost Voltage Divider

// Led Current Sense Measurement Resister
// LED3     P03      //  7 P0.03 AIN4 Green
// LED2     P01      //  5 P0.01 AIN2 Red  Switch 1.3/1.3 VBAT Divider
// LED1     P05      //  9 P0.05 AIN6 Blue

// LED Current Sense Resistor Pull Down
// LED3     P30      //  3 P0.30
// LED2     P28      // 47 P0.28
// LED1     P07      // 11 P0.07

#define PLATFORM_HAS_FSPI

enum {
    FSPI_SSEL = 29,
    FSPI_SCLK = 12, // 13,
    FSPI_MOSI = 14, // 15,
    FSPI_MISO = 25,
};

// FRAM SPI powered from VRGB requires 2.0V min to 3.6V max
// FRAM_SSEL   P29      // 48 P0.29
// FRAM_SCLK   P13      // 19 P0.13
// FRAM_MOSI   P15      // 21 P0.15
// FRAM_MISO   P25      // 44 P0.25

