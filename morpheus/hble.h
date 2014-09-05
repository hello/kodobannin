#pragma once
#include <nrf_sdm.h>
#include <nrf_soc.h>

#include <ble.h>

typedef void(*hble_evt_handler_t)(ble_evt_t* ble_evt);

void hble_bond_manager_init();
void hble_stack_init();
void hble_params_init(char* device_name);
void hble_advertising_init(ble_uuid_t service_uuid);
void hble_advertising_start();
void hble_set_advertising_mode(bool pairing_mode);
void hble_erase_other_bonded_central();
bool hble_uint64_to_hex_device_id(uint64_t device_id, char* hex_device_id, size_t* len);
bool hble_hex_to_uint64_device_id(const char* hex_device_id, uint64_t* device_id);