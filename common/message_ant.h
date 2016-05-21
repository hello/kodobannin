#pragma once
#include "message_base.h"
#include "ant_driver.h"
#include "ant_packet.h"

/**
 *
 * HLO ANT Air Protocol Constraints v0.2
 * 1. Header data is always sent as page:0
 * 2. Page count > 0 if data, otherwise is a special single packet command
 * 3. crc16 is computed on receive of every header page
 * 4. if header page's crc changes, that means new message has arrived
 * 
 * 5. TX ends message by transmitting message N times (defined by user)
 * 6. RX ends message by either completing checksum, or channel closes
 *    - In case of master, receiving an invalid checksum will result in loss packet.  
 *    - Further protocol are user defined
 *
 * Below is the structure of the ANT air packet (8 Bytes)
 * Table of page + page_count combinations:
 * +----------------------------------------------------+
 * |              Multi Frame Packets                   |
 * +----------------------------------------------------+
 * |   page   |page_count|function                      |
 * +----------------------------------------------------+
 * |    0     |   N > 0  |standard message header packet|
 * |0 < n <= N|   N > 0  |standard message payload      |
 * +----------------------------------------------------+
 **/

#define ANT_PROTOCOL_VER      (4)


typedef enum{
    MSG_ANT_PING = 0,
    MSG_ANT_TRANSMIT,
    MSG_ANT_HANDLE_MESSAGE,
    MSG_ANT_TRANSMIT_RECEIVE,
}MSG_ANT_Commands;

typedef struct{
    hlo_ant_device_t device;
    MSG_Data_t * message;
}MSG_ANT_Message_t;

typedef struct 
{
    uint8_t battery_level;
    uint8_t firmware_version;
    uint32_t uptime_sec;
}__attribute__((packed)) pill_heartbeat_t;

typedef struct{
    uint8_t counter;
}__attribute__((packed)) pill_shakedata_t;

typedef struct{
    uint32_t cap[2];
    uint32_t reserved;
}__attribute__((packed)) pill_proxdata_t;

typedef enum {
    ANT_PILL_DATA = 0,
    ANT_PILL_HEARTBEAT,
    ANT_PILL_SHAKING,
    ANT_PILL_DATA_ENCRYPTED,
    ANT_PILL_PROX_ENCRYPTED,
    ANT_PILL_PROX_PLAINTEXT,
    ANT_SENSE_RESPONSE_HEARTBEAT,
}MSG_ANT_PillDataType_t;

typedef struct{
    uint8_t type; //payload type using MSG_ANT_PillDataType;
    uint8_t version;
    uint16_t payload_len;
    uint64_t UUID;
    uint8_t payload[0];
}__attribute__((packed)) MSG_ANT_PillData_t;

typedef struct{
    /* Called when a known and connected device sends a message */
    void (*on_message)(const hlo_ant_device_t * id, MSG_Data_t * msg);
    /* Called when an ant initiates a connection, allocate(but don't release) a response if needed*/
    MSG_Data_t * INCREF (*on_connection)(const hlo_ant_device_t * id); 
}MSG_ANTHandler_t;

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent, const MSG_ANTHandler_t * handler,hlo_ant_role role, uint8_t device_type);
/* Helper API an Object based on type */
MSG_Data_t * INCREF AllocateEncryptedAntPayload(MSG_ANT_PillDataType_t type, void * payload, size_t len);
MSG_Data_t * INCREF AllocateAntPayload(MSG_ANT_PillDataType_t type, void * payload, size_t len);
