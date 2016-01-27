#pragma once
#include "message_base.h"

MSG_Status init_prox(void);
void read_prox(uint32_t * out_val1, uint32_t * out_val4);
