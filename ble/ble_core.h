#pragma once

#include <ble_gap.h>

typedef void (*services_init_t)();

void ble_init(services_init_t services_init);
ble_gap_sec_params_t* ble_gap_sec_params_get();
void ble_gap_sec_params_init(void);
void ble_gap_params_init(char* device_name);
