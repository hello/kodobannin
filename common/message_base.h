#pragma once
/**
 * Messaging framework type declarations.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "message_config.h"


typedef struct{
    uint32_t len;
    uint8_t  ref;
    uint8_t  buf[31];
}MSG_Data_t;

typedef enum{
    SUCCESS = 0,
    FAIL,
    OOM
}MSG_Status;

/**
 * Message object.
 * All modules that implements message capability must define the struct
 * including this one( which other modules use to push message)
 */
typedef struct{
    MSG_ModuleType type;
    char * typestr;
    MSG_Status ( *init ) ( void );
    MSG_Status ( *destroy ) ( void );
    MSG_Status ( *flush ) ( void );
    //probably need a source parameter
    MSG_Status ( *send ) ( MSG_ModuleType src, MSG_Data_t * data  );
}MSG_Base_t;
/*
 *
 * Message central object
 * all Base objects will send messages to central and then dispatch to respective modules.
 */
typedef struct{
    MSG_Status ( *send )(MSG_ModuleType src, MSG_ModuleType dst, MSG_Data_t * data);
    MSG_Status ( *loadmod )(MSG_Base_t * mod);
    MSG_Status ( *unloadmod )(MSG_Base_t * mod);
}MSG_Central_t;

MSG_Data_t * MSG_Base_AllocateDataAtomic(uint32_t size);
MSG_Data_t * MSG_Base_AllocateStringAtomic(const char * str);

MSG_Status MSG_Base_AcquireDataAtomic(MSG_Data_t * d);
MSG_Status MSG_Base_ReleaseDataAtomic(MSG_Data_t * d);


#define MSG_SEND_TO_UART(r,s) do{ \
    MSG_Data_t * tmp = MSG_Base_AllocateStringAtomic(s); \
    r->send(0,UART, tmp);\
    MSG_Base_ReleaseDataAtomic(tmp); \
    }while(0)
    
    


