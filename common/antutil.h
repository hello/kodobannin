#pragma once
#include "app.h"
#include <nrf_soc.h>

#define ANTPLUS_RF_FREQ               0x39u             /**< Frequency, Decimal 57 (2457 MHz). */

#define GET_UUID_32() (NRF_FICR->DEVICEID[0] ^ NRF_FICR->DEVICEID[1])
#define GET_UUID_16() ((uint16_t) (GET_UUID_32() & 0xFFFF)^((GET_UUID_32() >> 16) & 0xFFFF))

#define SET_DISCOVERY_PROFILE(pb) do{\
    if(pb){\
    }\
}while(0)

#define BIAS_RF_PERIOD( _base, _uuid32 ) do{\
    uint32_t base = _base; \
    uint16_t bias = (_uuid32) % 16;\
    base += bias; \
    if(base > 0xFFFF) base = 0xFFFF;\
    _base = (uint16_t)base;\
}while(0)
