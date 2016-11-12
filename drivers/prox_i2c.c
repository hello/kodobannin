#include "prox_i2c.h"
#include "twi_master.h"
#include "twi_master_config.h"
#include "util.h"
#include "stdint.h"
#include "stdbool.h"
#include <nrf_delay.h>
#include <nrf_gpio.h>

#define I2C_DELAY   15

#define FDC_ADDRESS (0b1010000)
/*
 *#define FDC_ADDRESS_WRITE (FDC_ADDRESS << 1)
 *#define FDC_ADDRESS_READ ((FDC_ADDRESS << 1) | TWI_READ_BIT)
 */
#define MANU_ID_ADDRESS 0xFE
#define DEVICE_ID_ADDRESS 0xFF
#define MANU_ID_VAL 0x5449
#define DEV_ID_VAL 0x1004


static inline void TWI_WRITE(uint8_t addr, const void * ptr, size_t ptr_size){
    twi_master_transfer( (addr << 1), (void*)ptr, ptr_size, TWI_ISSUE_STOP);
    nrf_delay_ms(I2C_DELAY);
}

static inline void TWI_READ(uint8_t addr, uint8_t reg, void * ptr, size_t ptr_size){
    uint8_t r = (uint8_t)reg;
    TWI_WRITE(addr, &r, sizeof(r));
    nrf_delay_ms(I2C_DELAY);
    twi_master_transfer( ((addr << 1) | TWI_READ_BIT), ptr, ptr_size, TWI_ISSUE_STOP);
    nrf_delay_ms(I2C_DELAY);
}


static const uint8_t CONF_MEAS1[3] = { 0x08, 0x13, 0x40 };
//static const uint8_t CONF_MEAS1[3] = { 0x08, 0x0C, 0x00 };
static const uint8_t CONF_MEAS4[3] = { 0x0B, 0x73, 0x40 }; //72

static uint8_t CONF_GAIN_CAL1[3] = {0x11, 0x20, 0x00};// CIN1 4x gain as 0xFF
static uint8_t CONF_GAIN_CAL4[3] = {0x14, 0x20, 0x00};// CIN4 4x gain as 0xFF

static uint8_t CONF_OFFSET_CAL1[3] = {0x0D, 0x00, 0x00};// 9 pF
static uint8_t CONF_OFFSET_CAL4[3] = {0x10, 0x00, 0x00};// 6 pF

static const uint8_t CONF_READ1[3] = {0x0C, 0x04, 0x80};//config nonrepeat 
static const uint8_t CONF_READ4[3] = {0x0C, 0x04, 0x10};//config nonrepeat 
#define READ_1_ADDRESS_HI  0x00
#define READ_1_ADDRESS_LO  0x01
#define READ_4_ADDRESS_HI  0x06
#define READ_4_ADDRESS_LO  0x07

#define swap_endian16(x) ( (x >> 8) | ((x & 0xff) << 8) )
static uint32_t _byte_check(const uint8_t * compare, const uint8_t * actual, size_t sz){
    int i;
    for(i = 0; i < sz; i++){
        if ( compare[i] != actual[i] ){
            PRINTS("Byte Check Failed, ");
            PRINT_HEX(compare, sz);
            PRINTS(" | ");
            PRINT_HEX(actual, sz);
            PRINTS("\r\n");
            return 1;
        }
    }
    return 0;
}
static void _reset_config(void){
    uint8_t RST[3] = {0x0C, 0x84, 0x90};
    TWI_WRITE(FDC_ADDRESS, RST, sizeof(RST));
}
static MSG_Status _check_id(void){
    uint16_t r = 0;
    uint16_t MANU_ID = MANU_ID_VAL;
    uint16_t DEV_ID = DEV_ID_VAL;
    TWI_READ(FDC_ADDRESS, MANU_ID_ADDRESS, &r, sizeof(r));
    r = swap_endian16(r);
    if( 0 != _byte_check((uint8_t*)&r, (uint8_t*)&MANU_ID, sizeof(r)) ){
        return FAIL;
    }

    TWI_READ(FDC_ADDRESS, DEVICE_ID_ADDRESS, &r, sizeof(r));
    r = swap_endian16(r);
    if(0 != _byte_check((uint8_t*)&r, (uint8_t*)&DEV_ID, sizeof(r)) ){
        return FAIL;
    }
    return SUCCESS;
}
static void _conf_prox(void){
    uint8_t r[2] = {0};
    TWI_WRITE(FDC_ADDRESS, CONF_MEAS1, sizeof(CONF_MEAS1));
    TWI_WRITE(FDC_ADDRESS, CONF_MEAS4, sizeof(CONF_MEAS4));
    //config offset
    TWI_WRITE(FDC_ADDRESS, CONF_OFFSET_CAL1, sizeof(CONF_OFFSET_CAL1));
    TWI_WRITE(FDC_ADDRESS, CONF_OFFSET_CAL4, sizeof(CONF_OFFSET_CAL4));
    //config gain
    TWI_WRITE(FDC_ADDRESS, CONF_GAIN_CAL1, sizeof(CONF_GAIN_CAL1));
    TWI_WRITE(FDC_ADDRESS, CONF_GAIN_CAL4, sizeof(CONF_GAIN_CAL4));
    //verify
    TWI_READ(FDC_ADDRESS, CONF_MEAS1[0], r,sizeof(r));
    APP_OK( _byte_check(r, &CONF_MEAS1[1], sizeof(r)) );
    TWI_READ(FDC_ADDRESS, CONF_MEAS4[0], r,sizeof(r));
    APP_OK( _byte_check(r, &CONF_MEAS4[1], sizeof(r)) );
}

static MSG_Status _prox_power(uint8_t on){
#ifdef PLATFORM_HAS_PROX
    if(on){
        nrf_gpio_cfg_output(PROX_BOOST_ENABLE);
        nrf_gpio_cfg_output(PROX_BOOST_MODE);
        nrf_gpio_cfg_output(PROX_VDD_EN);

        nrf_gpio_pin_set(PROX_BOOST_ENABLE);
        nrf_gpio_pin_set(PROX_BOOST_MODE);
        nrf_gpio_pin_set(PROX_VDD_EN);

        nrf_delay_ms(8 * I2C_DELAY);
        if(SUCCESS != _check_id()){
            PRINTF("Unable to check id");
            return FAIL;
        }
        _reset_config();
        _conf_prox();
    }else{
        nrf_gpio_cfg_output(PROX_BOOST_ENABLE);
        nrf_gpio_cfg_output(PROX_BOOST_MODE);
        nrf_gpio_cfg_output(PROX_VDD_EN);

        nrf_gpio_pin_clear(PROX_BOOST_ENABLE);
        nrf_gpio_pin_clear(PROX_BOOST_MODE);
        nrf_gpio_pin_clear(PROX_VDD_EN);
    }
    return SUCCESS;
#endif
    return FAIL;
}

MSG_Status init_prox(void){
    MSG_Status ret = _prox_power(1);
    _prox_power(0);
    return ret;
}

void read_prox(int32_t * out_val1, int32_t * out_val4){
#ifdef PLATFORM_HAS_PROX
    uint16_t cap_meas1_hi = 0;
    uint16_t cap_meas1_lo = 0;
    uint16_t cap_meas4_hi = 0;
    uint16_t cap_meas4_lo = 0;
    uint32_t cap_meas1_raw = 0;
    uint32_t cap_meas4_raw = 0;
    uint16_t res = 0;

    _prox_power(1);

    TWI_WRITE(FDC_ADDRESS, CONF_READ1, sizeof(CONF_READ1));
    do {
        TWI_READ(FDC_ADDRESS, 0x0C, &res, sizeof(res));
        res = swap_endian16(res);
        nrf_delay_ms(10);
    }while (!(res & 0x08));
    TWI_READ(FDC_ADDRESS, READ_1_ADDRESS_HI, &cap_meas1_hi, 2);
    TWI_READ(FDC_ADDRESS, READ_1_ADDRESS_LO, &cap_meas1_lo, 2);

    TWI_WRITE(FDC_ADDRESS, CONF_READ4, sizeof(CONF_READ4));
    do {
        TWI_READ(FDC_ADDRESS, 0x0C, &res, sizeof(res));
        res = swap_endian16(res);
        nrf_delay_ms(10);
    }while (!(res & 0x01));
    TWI_READ(FDC_ADDRESS, READ_4_ADDRESS_HI, &cap_meas4_hi, 2);
    TWI_READ(FDC_ADDRESS, READ_4_ADDRESS_LO, &cap_meas4_lo, 2);

    cap_meas1_raw = swap_endian16(cap_meas1_lo) | (swap_endian16(cap_meas1_hi) << 16);
    cap_meas1_raw = cap_meas1_raw >> 8;
    cap_meas4_raw = swap_endian16(cap_meas4_lo) | (swap_endian16(cap_meas4_hi) << 16);
    cap_meas4_raw = cap_meas4_raw >> 8;
    if(cap_meas4_raw & 1<<23){
        cap_meas4_raw |= 0xFF000000;
    }
    if(cap_meas1_raw & 1<<23){
        cap_meas1_raw |= 0xFF000000;
    }
    *out_val1 = (uint32_t)((cap_meas1_raw));
    *out_val4 = (uint32_t)((cap_meas4_raw));

    _prox_power(0);
#endif
}
void set_prox_offset(int16_t off1, int16_t off4){
    CONF_OFFSET_CAL1[1] = (off1 & 0x1F) << 3;
    CONF_OFFSET_CAL1[2] = (off1 & 0xFF00) >> 8;

    CONF_OFFSET_CAL4[1] = (off4 & 0x1F) << 3;
    CONF_OFFSET_CAL4[2] = (off4 & 0xFF00) >> 8;
}
void set_prox_gain(uint16_t gain1, uint16_t gain4){
    CONF_GAIN_CAL1[1] = (gain1 & 0xFF00) >> 8;
    CONF_GAIN_CAL1[2] = gain1 & 0xFF;

    CONF_GAIN_CAL4[1] = (gain4 & 0xFF00) >> 8;
    CONF_GAIN_CAL4[2] = gain4 & 0xFF;
}
