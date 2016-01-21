// vi:noet:sw=4 ts=4

#pragma once

#ifdef ANT_STACK_SUPPORT_REQD
#define ANT_ENABLE
#endif

#define BLE_ENABLE

#define HW_REVISION 3

#define DEVICE_KEY_ADDRESS                   0x20003FF0
//use hello's ant network key
#define USE_HLO_ANT_NETWORK

#define configTOTAL_HEAP_SIZE 1280

#define PLATFORM_HAS_SERIAL

enum {
    SERIAL_TX_PIN = 18,
    SERIAL_RX_PIN = 19,
    SERIAL_CTS_PIN = 31,
    SERIAL_RTS_PIN = 31,
};


#define PLATFORM_HAS_IMU
#define PLATFORM_HAS_IMU_VDD_CONTROL

enum {
    IMU_VDD_EN   = 20,

    IMU_SPI_nCS  = 16,
    IMU_SPI_SCLK = 14,
    IMU_SPI_MOSI = 12,
    IMU_SPI_MISO = 10,

    IMU_INT = 23,
};

#define PLATFORM_HAS_BATTERY
#define PLATFORM_HAS_VERSION

enum {
    VBAT_SENSE  = 6,
    VMCU_SENSE  = 2,

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
    VLED_VDD_EN  = 17,
    VRGB_ENABLE  = 11,

    VRGB_SELECT  =  8,
    VRGB_ADJUST  =  9,
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
