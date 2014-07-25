#pragma once

typedef void(*hble_evt_handler_t)(ble_evt_t* ble_evt);

void bond_manager_init();
void hble_init(nrf_clock_lfclksrc_t clock_source, bool use_scheduler, char* device_name, hble_evt_handler_t ble_evt_handler);
void hble_advertising_init(ble_uuid_t service_uuid);
void hble_advertising_start();
