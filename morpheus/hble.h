#pragma once
#include <nrf_sdm.h>
#include <nrf_soc.h>

#include <ble.h>

typedef void(*hble_evt_handler_t)(ble_evt_t* ble_evt);

void hble_bond_manager_init();
void hble_stack_init(nrf_clock_lfclksrc_t clock_source, bool use_scheduler);
void hble_params_init(char* device_name);
void hble_advertising_init(ble_uuid_t service_uuid);
void hble_advertising_start();
void hble_set_advertising_mode(bool pairing_mode);