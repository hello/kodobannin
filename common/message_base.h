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
#ifdef MSG_BASE_DATA_BUFFER_SIZE
    uint8_t  buf[MSG_BASE_DATA_BUFFER_SIZE];
#else
    uint8_t * buf;
#endif
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
    MSG_Status ( *dispatch )(MSG_ModuleType src, MSG_ModuleType dst, MSG_Data_t * data);
    MSG_Status ( *loadmod )(MSG_Base_t * mod);
    MSG_Status ( *unloadmod )(MSG_Base_t * mod);
}MSG_Central_t;

MSG_Data_t * MSG_Base_AllocateDataAtomic(uint32_t size);
MSG_Data_t * MSG_Base_AllocateStringAtomic(const char * str);
MSG_Data_t * MSG_Base_Orig(void * data);

MSG_Status MSG_Base_AcquireDataAtomic(MSG_Data_t * d);
MSG_Status MSG_Base_ReleaseDataAtomic(MSG_Data_t * d);


#define MSG_PING(c,r,i) do{ \
    MSG_Data_t * tmp = MSG_Base_AllocateDataAtomic(1); \
    tmp->buf[0] = i; \
    if(c) c->dispatch(0,r, tmp);\
    MSG_Base_ReleaseDataAtomic(tmp); \
    }while(0)
    
#define MSG_SEND(central, recipient, command, payload, len) do{\
    MSG_Data_t * obj = MSG_Base_AllocateDataAtomic(len + 1);\
    if(obj){\
        if(central){\
            obj->buf[0] = command;\
            memcpy(obj->buf+1, payload,len);\
            central->dispatch(0,recipient,obj);\
        }\
        MSG_Base_ReleaseDataAtomic(obj);\
    }\
}while(0)
