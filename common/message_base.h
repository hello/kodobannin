/**
 * Messaging framework type declarations.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#ifndef MESSAGE_H
#define MESSAGE_H

typedef struct{
    uint32_t len;
    uint8_t  buf[];
}MSG_Data_t;

typedef enum{
    SLAVE = 0,
    MASTER
}MSG_Role;

typedef enum{
    MEM = 0,
    UART,
    SPI,
    BTLE,
    ANT
}MSG_PipeType;

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
    MSG_PipeType type;
    MSG_Role role;
    MSG_Status ( *init ) ( void );
    MSG_Status ( *destroy ) ( void );
    MSG_Status ( *flush ) ( void );
    //probably need a source parameter
    MSG_Status ( *send ) (  MSG_Data_t * data  );
}MSG_Base_t;

#endif
