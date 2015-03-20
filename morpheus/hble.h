#pragma once
#include <nrf_sdm.h>
#include <nrf_soc.h>
#include <ble_bondmngr.h>
#include <ble.h>

#define MAX_DELAY_TASKS         7

typedef void(*hble_evt_handler_t)(ble_evt_t* ble_evt);
typedef void(*delay_task_t)(void);
typedef void(*bond_status_callback_t)(ble_bondmngr_evt_type_t t);
typedef void(*connected_callback_t)(void);
typedef void(*on_advertise_started_callback_t)(bool is_pairing, uint16_t bond_count);

void hble_bond_manager_init();
void hble_stack_init();
void hble_params_init(const char* device_name, uint64_t device_id, uint32_t middle_board_ver);
void hble_services_init(void);
void hble_advertising_init(ble_uuid_t service_uuid);
void hble_advertising_start();
void hble_set_advertising_mode(bool pairing_mode);
void hble_erase_other_bonded_central();
void hble_erase_all_bonded_central();
bool hble_uint64_to_hex_device_id(uint64_t device_id, char* hex_device_id, size_t* len);
bool hble_hex_to_uint64_device_id(const char* hex_device_id, uint64_t* device_id);
uint64_t hble_get_device_id();

bool hble_set_delay_task(uint8_t index, const delay_task_t task);

void hble_set_advertise_callback(on_advertise_started_callback_t cb);
void hble_set_bond_status_callback(bond_status_callback_t callback);
void hble_set_connected_callback(connected_callback_t callback);

void hble_delay_tasks_erase_bonds();
void hble_delay_task_advertise_resume();
void hble_erase_1st_bond();

