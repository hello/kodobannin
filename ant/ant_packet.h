#pragma once
/**
 * packet assember that outputs MSG_Data object
 */
#include "ant_driver.h"
#include "message_base.h"

#ifndef ANT_PACKET_MAX_CONCURRENT_SESSIONS
#define ANT_PACKET_MAX_CONCURRENT_SESSIONS 8
#endif

typedef struct{
    void (*on_message)(const hlo_ant_device_t * device, MSG_Data_t * message);
    void (*on_message_sent)(const hlo_ant_device_t * device, MSG_Data_t * message);
}hlo_ant_packet_listener;

hlo_ant_event_listener_t * hlo_ant_packet_init(const hlo_ant_packet_listener * user_listener);
//no queue enabled, returns error if sending
int hlo_ant_packet_send_message(const hlo_ant_device_t * device, MSG_Data_t * msg);
