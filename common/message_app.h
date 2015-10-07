/*
 * Application that implements central node 
 */
#ifndef MESSAGE_APP
#define MESSAGE_APP
#include "message_base.h"
#include <app_timer.h>
#include <app_scheduler.h>

typedef enum{
    MSG_APP_PING = 0,
    MSG_APP_LSMOD,
}MSG_App_Commands;

MSG_Central_t * MSG_App_Central( app_sched_event_handler_t unknown_handler );
MSG_Base_t * MSG_App_Base(MSG_Central_t * parent);


#endif


