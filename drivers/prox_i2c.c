#include "prox_i2c.h"
#include "twi_master.h"
#include "twi_master_config.h"
#include "util.h"
#include "stdint.h"
#include "stdbool.h"
#include <nrf_delay.h>

#define I2C_DELAY   10

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
}

static inline void TWI_READ(uint8_t addr, uint8_t reg, void * ptr, size_t ptr_size){
    uint8_t r = (uint8_t)reg;
    TWI_WRITE(addr, &r, sizeof(r));
    nrf_delay_ms(I2C_DELAY);
    twi_master_transfer( ((addr << 1) | TWI_READ_BIT), ptr, ptr_size, TWI_ISSUE_STOP);
    nrf_delay_ms(I2C_DELAY);
}


static const uint8_t CONF_MEAS1[3] = { 0x08, 0x1C, 0x00 };
static const uint8_t CONF_MEAS4[3] = { 0x0B, 0x7C, 0x00 };

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
static void _check_id(void){
    uint16_t r = 0;
    uint16_t MANU_ID = MANU_ID_VAL;
    uint16_t DEV_ID = DEV_ID_VAL;
    TWI_READ(FDC_ADDRESS, MANU_ID_ADDRESS, &r, sizeof(r));
    r = swap_endian16(r);
    APP_OK( _byte_check((uint8_t*)&r, (uint8_t*)&MANU_ID, sizeof(r)) );
    TWI_READ(FDC_ADDRESS, DEVICE_ID_ADDRESS, &r, sizeof(r));
    r = swap_endian16(r);
    APP_OK( _byte_check((uint8_t*)&r, (uint8_t*)&DEV_ID, sizeof(r)) );
}
static void _conf_prox(void){
    uint8_t r[2] = {0};
    TWI_WRITE(FDC_ADDRESS, CONF_MEAS1, sizeof(CONF_MEAS1));
    TWI_WRITE(FDC_ADDRESS, CONF_MEAS4, sizeof(CONF_MEAS4));
    //verify
    TWI_READ(FDC_ADDRESS, CONF_MEAS1[0], r,sizeof(r));
    APP_OK( _byte_check(r, &CONF_MEAS1[1], sizeof(r)) );
    TWI_READ(FDC_ADDRESS, CONF_MEAS4[0], r,sizeof(r));
    APP_OK( _byte_check(r, &CONF_MEAS4[1], sizeof(r)) );
}


MSG_Status init_prox(void){
    _reset_config();
    _check_id();
    _conf_prox();
    return SUCCESS;
}

void read_prox(uint16_t * out_val1, uint16_t * out_val4){
    uint16_t cap_meas1_hi = 0;
    uint16_t cap_meas1_lo = 0;
    uint16_t cap_meas4_hi = 0;
    uint16_t cap_meas4_lo = 0;
    TWI_WRITE(FDC_ADDRESS, CONF_READ1, sizeof(CONF_READ1));
    TWI_WRITE(FDC_ADDRESS, CONF_READ4, sizeof(CONF_READ4));
    //todo verify write
    TWI_READ(FDC_ADDRESS, READ_1_ADDRESS_HI, &cap_meas1_hi, 2);
    TWI_READ(FDC_ADDRESS, READ_1_ADDRESS_LO, &cap_meas1_lo, 2);
    TWI_READ(FDC_ADDRESS, READ_4_ADDRESS_HI, &cap_meas4_hi, 2);
    TWI_READ(FDC_ADDRESS, READ_4_ADDRESS_LO, &cap_meas4_lo, 2);

    *out_val1 = swap_endian16(cap_meas1_lo) | (swap_endian16(cap_meas1_hi) << 16);
    *out_val4 = swap_endian16(cap_meas4_lo) | (swap_endian16(cap_meas4_hi) << 16);
}
