#pragma once
/**
 * packet assember that outputs MSG_Data object
 */
#include "ant_driver.h"
#include "message_base.h"

#ifndef ANT_PACKET_MAX_CONCURRENT_SESSIONS
#define ANT_PACKET_MAX_CONCURRENT_SESSIONS 4
#endif

typedef struct{
    void (*on_message)(const hlo_ant_device_t * device, MSG_Data_t * message);
    void DECREF (*on_message_sent)(const hlo_ant_device_t * device, MSG_Data_t * message);
    void DECREF (*on_message_failed)(const hlo_ant_device_t * device);
}hlo_ant_packet_listener;

hlo_ant_event_listener_t * hlo_ant_packet_init(const hlo_ant_packet_listener * user_listener);
//no queue enabled, returns error if sending
int INCREF hlo_ant_packet_send_message(const hlo_ant_device_t * device, MSG_Data_t * msg);
