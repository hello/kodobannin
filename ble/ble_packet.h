#ifndef BLE_PACKET_H
/**
 * Converts packet to MSG_Data
 */

#include "message_base.h"
#include "ble.h"
typedef void (*message_handler)(uint16_t handle, MSG_Data_t * msg);

void ble_packet_set_handler(message_handler handler);
void ble_packet_write_handler(ble_gatts_evt_write_t* event);

#endif
