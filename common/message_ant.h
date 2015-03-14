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

#define ANT_PROTOCOL_VER      (3)


typedef enum{
    MSG_ANT_PING = 0,
    MSG_ANT_CONNECTION_BASE,
    MSG_ANT_SET_ROLE = 100,
    MSG_ANT_REMOVE_DEVICE,
    MSG_ANT_ADD_DEVICE,
    MSG_ANT_HANDLE_MESSAGE,
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

typedef enum {
    ANT_PILL_DATA = 0,
    ANT_PILL_HEARTBEAT,
    ANT_PILL_SHAKING,
    ANT_PILL_DATA_ENCRYPTED
}MSG_ANT_PillDataType_t;

typedef struct{
    uint8_t type; //payload type using MSG_ANT_PillDataType;
    uint8_t version;
    uint16_t payload_len;
    uint64_t UUID;
    uint8_t payload[0];
}__attribute__((packed)) MSG_ANT_PillData_t;

/**
 * ANT user defined actions
 * all callbacks must be defined(can not be NULL)
 **/
typedef enum{
    ANT_STATUS_NULL = 0,
    ANT_STATUS_DISCONNECTED,
    ANT_STATUS_CONNECTED
}ANT_Status_t;

typedef struct{
    /* Called when a known and connected device sends a message */
    void (*on_message)(const hlo_ant_device_t * id, MSG_Address_t src, MSG_Data_t * msg);

    void (*on_status_update)(const hlo_ant_device_t * id, ANT_Status_t status);
}MSG_ANTHandler_t;

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent, const MSG_ANTHandler_t * handler);
