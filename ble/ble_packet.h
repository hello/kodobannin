#ifndef BLE_PACKET_H
/**
 * Converts packet to MSG_Data and vice versa
 */

#include "message_base.h"
#include "ble.h"
typedef void (*ble_packet_message_handler)(uint16_t uuid, MSG_Data_t * msg);

//---------------------------
//app side
//sets on message handler for incoming message
void ble_packet_set_handler(ble_packet_message_handler handler);

//---------------------------
//sdk side
//callback for characteristic write(master -> peripheral)
//emits a complete MSG_Data to message_handler when completed
void ble_packet_write_handler(ble_gatts_evt_write_t* event);
void ble_packet_reset(void);


#endif
