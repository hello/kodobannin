#pragma once
#include "message_base.h"

#define ANT_DISCOVERY_CHANNEL 0
typedef struct{
    uint8_t hlo_hw_type;
    uint8_t hlo_hw_revision;
    uint8_t hlo_version_major;
    uint8_t hlo_version_minor;
    uint32_t UUID;//4
    uint64_t monotonic_time;
}ANT_DiscoveryProfile_t;

typedef struct{
    uint8_t transmit_type;
    uint8_t device_type;
    uint16_t device_number;
}ANT_ChannelID_t;

typedef struct{
    //cached status
    uint8_t reserved;
    //MASTER/SLAVE/SHARED etc
    uint8_t channel_type;
    //2.4GHz + frequency, 2.4XX
    uint8_t frequency;
    //network key, HLO specific
    uint8_t network;
    //period 32768/period Hz
    uint16_t period;
}ANT_ChannelPHY_t;

typedef struct{
}ANT_Channel_t;

typedef struct{
    enum{
        ANT_PING=0,
        ANT_SET_ROLE,//sets discovery role
        ANT_SET_DISCOVERY_PROFILE
    }cmd;
    union{
        uint8_t role;
        ANT_DiscoveryProfile_t profile;
    }param;
}MSG_ANTCommand_t;

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent);
