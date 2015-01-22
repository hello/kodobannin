#pragma once

typedef void(*hble_evt_handler_t)(ble_evt_t* ble_evt);

void hble_bond_manager_init();
void hble_stack_init();
void hble_params_init(char* device_name);
void hble_advertising_init(ble_uuid_t service_uuid);
void hble_advertising_start();
