#pragma once
/**
 * Messaging framework type declarations.
 *
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "message_config.h"

/**
 * Flags for functions that modifies reference
 **/
#define INCREF
#define DECREF
/**
 * MSG_Data Object context flags
 * Modules should try to conform to these flags
 **/
#define MSG_DATA_CTX_READ_ONLY 0x01 /* Read only Object */
#define MSG_DATA_CTX_META_DATA 0x02 /* Contains metadata instead of normal data */
#define MSG_DATA_CTX_RETURN_TO_SENDER 0x80 /* not implemented yet */

typedef struct _MSG_Data_t{
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
}__attribute__((packed)) MSG_Data_t;


typedef enum{
    CENTRAL = 0,
    UART,
    IMU,
    BLE,
    ANT,
    RTC,
    CLI,
    TIME,
    SSPI,
    LED,
    MOD_END
}MSG_ModuleType;


typedef enum{
    SUCCESS = 0,
    FAIL,
    OOM
}MSG_Status;

typedef struct{
    uint8_t module;
    uint8_t submodule;
}MSG_Address_t;

#define ADDR(a,b) ((MSG_Address_t){a,b})
/**
 * Message object.
 * All modules that implements message capability must define the struct
 * including this one( which other modules use to push message)
 */
typedef struct{
    MSG_ModuleType type;
    const char * typestr;
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


MSG_Data_t * INCREF MSG_Base_AllocateDataAtomic(size_t size);
MSG_Data_t * INCREF MSG_Base_AllocateStringAtomic(const char * str);
MSG_Data_t * INCREF MSG_Base_AllocateObjectAtomic(const void * obj, size_t size);
MSG_Data_t * INCREF MSG_Base_Dupe(MSG_Data_t * orig);
MSG_Status MSG_Base_BufferTest(void);
uint32_t MSG_Base_FreeCount(void);
uint32_t MSG_Base_BigPoolFreeCount(void);
bool MSG_Base_HasMemoryLeak(void);

MSG_Status   INCREF MSG_Base_AcquireDataAtomic(MSG_Data_t * d);
MSG_Status   DECREF MSG_Base_ReleaseDataAtomic(MSG_Data_t * d);


/**
 * Message queue object
 * Use to queue up onjects for async operation
 */
typedef struct{
    uint8_t capacity;
    uint8_t elements;
    uint8_t rdx;
    uint8_t wdx;
    MSG_Data_t * q[];
}MSG_Queue_t;

MSG_Queue_t * MSG_Base_InitQueue(void * mem, uint32_t size);
MSG_Status INCREF MSG_Base_QueueAtomic(MSG_Queue_t * queue, MSG_Data_t * obj);
MSG_Data_t * DECREF MSG_Base_DequeueAtomic(MSG_Queue_t * queue);

