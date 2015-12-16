#ifndef MESSAGE_PROX_H
#define MESSAGE_PROX_H
#include "message_base.h"

MSG_Base_t * MSG_Prox_Init(const MSG_Central_t * central);

uint16_t MSG_Prox_Read(void);

#endif
