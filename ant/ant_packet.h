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
    MSG_Data_t* INCREF (*on_connect)(const hlo_ant_device_t * device);                          //called when central receives a header packet
    void (*on_message)(const hlo_ant_device_t * device, MSG_Data_t * message);                  //called when device receives a complete message
    void DECREF (*on_message_sent)(const hlo_ant_device_t * device, MSG_Data_t * message);      //called when messag has been sent
    void DECREF (*on_message_failed)(const hlo_ant_device_t * device, MSG_Data_t * message);    //called on failed transmission
}hlo_ant_packet_listener;

hlo_ant_event_listener_t * hlo_ant_packet_init(const hlo_ant_packet_listener * user_listener);
//no queue enabled, returns error if sending
int INCREF hlo_ant_packet_send_message(const hlo_ant_device_t * device, MSG_Data_t * msg, bool full_duplex);
