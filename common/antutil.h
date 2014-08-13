#pragma once
#include "app.h"
#include <nrf_soc.h>

#define ANTPLUS_RF_FREQ               0x39u             /**< Frequency, Decimal 57 (2457 MHz). */

#define GET_UUID_32() (NRF_FICR->DEVICEID[0] ^ NRF_FICR->DEVICEID[1])
#define GET_UUID_16() ((uint16_t) (GET_UUID_32() & 0xFFFF)^((GET_UUID_32() >> 16) & 0xFFFF))

#define SET_DISCOVERY_PROFILE(pb) do{\
    if(pb){\
        ANT_DiscoveryProfile_t * profile = (ANT_DiscoveryProfile_t*)pb->buf;\
        profile->UUID = NRF_FICR->DEVICEID[0] ^ NRF_FICR->DEVICEID[1];\
        profile->hlo_identifier = ANT_UNIQ_ID;\
        profile->hlo_hw_type = ANT_HW_TYPE;\
        profile->hlo_hw_revision = ANT_HW_REVISION;\
        profile->hlo_version_major = ANT_SW_MAJOR;\
        profile->hlo_version_minor = ANT_SW_MINOR;\
        profile->phy.channel_type = ANT_PREFER_CHTYPE;\
        profile->phy.frequency = ANT_PREFER_FREQ;\
        profile->phy.network = ANT_PREFER_NETWORK;\
        profile->phy.period = ANT_PREFER_PERIOD;\
    }\
}while(0)

/**
 * 0 - 124
 * if _ret falls into ANT+ channel, increase it by 1
 **/
#define DERIVE_RF_FREQ( _ret, _uuidhost, _uuidclient ) do{\
    _ret = (uint8_t)((_uuidhost ^ _uuidclient) % 125);\
    if( _ret == ANTPLUS_RF_FREQ || _ret == 66 ){\
        _ret += 1;\
    }}while(0)
