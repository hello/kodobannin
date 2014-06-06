#pragma once

typedef void(*hble_evt_handler_t)(ble_evt_t* ble_evt);

void hble_init(nrf_clock_lfclksrc_t clock_source, bool use_scheduler, char* device_name, hble_evt_handler_t ble_evt_handler);
