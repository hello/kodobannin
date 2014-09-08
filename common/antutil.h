#pragma once
#include "app.h"
#include <nrf_soc.h>

#define ANTPLUS_RF_FREQ               0x39u             /**< Frequency, Decimal 57 (2457 MHz). */

#define SET_DISCOVERY_PROFILE(pb) do{\
    if(pb){\
    }\
}while(0)

#define BIAS_RF_PERIOD( _base, _uuid16 ) do{\
    uint32_t base = _base; \
    uint16_t bias = (_uuid16) % 16;\
    base += bias; \
    if(base > 0xFFFF) base = 0xFFFF;\
    _base = (uint16_t)base;\
}while(0)
