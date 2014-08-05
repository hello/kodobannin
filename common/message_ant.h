#pragma once
#include "message_base.h"

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
    }cmd;
    union{
        uint8_t role;
        uint8_t unpair_channel_mask;
        ANT_Channel_t cfg;
        uint8_t ch;
    }param;
}MSG_ANTCommand_t;

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent);
