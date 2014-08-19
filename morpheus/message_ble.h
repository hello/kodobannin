#pragma once
#include "message_base.h"


MSG_Base_t* MSG_BLE_Base(MSG_Central_t* parent);
MSG_Status route_data_to_cc3200(const void* data, size_t len);

