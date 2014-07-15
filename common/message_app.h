/*
 * Application that implements central node 
 */
#ifndef MESSAGE_APP
#define MESSAGE_APP
#include "message_base.h"
#include <app_timer.h>


typedef struct{
    enum{
        APP_PING=0,
        APP_LSMOD,
        APP_LOADBASE,
        APP_UNLOAD
    }cmd;
    union{
        MSG_Base_t * base;
        MSG_ModuleType unloadtype;
    }param;
}MSG_AppCommand_t;

MSG_Central_t * MSG_App_Central( app_sched_event_handler_t unknown_handler );
MSG_Base_t * MSG_App_Base(MSG_Central_t * parent);


#endif


