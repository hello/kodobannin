/*
 * exit node of messages from transport (and starting point of messages to transports)
 */
#ifndef MESSAGE_APP
#define MESSAGE_APP
#include "message_base.h"
#include <app_timer.h>


MSG_Base_t * MSG_App_Init( app_sched_event_handler_t message_handler );

MSG_Status MSG_App_Add_Receiver( MSG_Base_t * receiver );
MSG_Status MSG_App_Remove_Receiver( MSG_Base_t * receiver );
MSG_Status MSG_App_Send_All( void );

#endif


