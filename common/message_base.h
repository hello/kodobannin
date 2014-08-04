#pragma once
/**
 * Messaging framework type declarations.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "message_config.h"

#define INCREF
#define DECREF

typedef struct{
    /*
     * Length of the valid data in the buffer 
     */
    uint16_t len;
    /*
     * reference count, user do not modify
     */
    uint8_t  ref;
    /*
     * context specific register
     */
    uint8_t  context;
    /*
     * data buffer
     */
    uint8_t buf[0];
}MSG_Data_t;

typedef enum{
    SUCCESS = 0,
    FAIL,
    OOM
}MSG_Status;

typedef struct{
    MSG_ModuleType module;
    uint8_t submodule;
}MSG_Address_t;
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
    MSG_Status ( *send ) ( MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data  );
}MSG_Base_t;
/*
 *
 * Message central object
 * all Base objects will send messages to central and then dispatch to respective modules.
 */
typedef struct{
    MSG_Status ( *dispatch )(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data);
    MSG_Status ( *loadmod )(MSG_Base_t * mod);
    MSG_Status ( *unloadmod )(MSG_Base_t * mod);
}MSG_Central_t;

MSG_Data_t * INCREF MSG_Base_AllocateDataAtomic(uint32_t size);
MSG_Data_t * INCREF MSG_Base_AllocateStringAtomic(const char * str);
MSG_Status MSG_Base_BufferTest(void);

MSG_Status   INCREF MSG_Base_AcquireDataAtomic(MSG_Data_t * d);
MSG_Status   DECREF MSG_Base_ReleaseDataAtomic(MSG_Data_t * d);


#define MSG_PING(c,r,i) do{ \
    MSG_Data_t * tmp = MSG_Base_AllocateDataAtomic(1); \
    tmp->buf[0] = i; \
    if(c) c->dispatch((MSG_Address_t){0,0},(MSG_Address_t){r,0}, tmp);\
    MSG_Base_ReleaseDataAtomic(tmp); \
    }while(0)
    
#define MSG_SEND(central, recipient, command, payload, len) do{\
    MSG_Data_t * obj = MSG_Base_AllocateDataAtomic(len + 1);\
    if(obj){\
        if(central){\
            obj->buf[0] = command;\
            memcpy(obj->buf+1, payload,len);\
            central->dispatch((MSG_Address_t){0,0},(MSG_Address_t){recipient}, obj);\
        }\
        MSG_Base_ReleaseDataAtomic(obj);\
    }\
}while(0)

