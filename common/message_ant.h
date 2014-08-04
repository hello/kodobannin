#pragma once
#include "message_base.h"

typedef struct{
    enum{
        ANT_PING=0,
        ANT_SET_ROLE,
    }cmd;
    union{
        uint8_t role;
        uint8_t unpair_channel_mask;
    }param;
}MSG_ANTCommand_t;

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent);
