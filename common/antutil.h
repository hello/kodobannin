#pragma once
#include "app.h"
#include <nrf_soc.h>

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
