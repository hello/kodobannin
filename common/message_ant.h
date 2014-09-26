#pragma once
#include "message_base.h"
#include "ant_driver.h"
#include "ant_packet.h"

#define TF_CONDENSED_BUFFER_SIZE (3)

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

typedef struct{
    enum{
        ANT_PING=0,
        ANT_SET_ROLE,//sets device role
        ANT_REMOVE_DEVICE,
        ANT_ADD_DEVICE,
        ANT_END_CMD,
    }cmd;
    union{
        hlo_ant_role role;
        hlo_ant_device_t device;
    }param;
}MSG_ANTCommand_t;


typedef struct 
{
    uint8_t battery_level;
    uint32_t uptime_sec;
}__attribute__((packed)) pill_heartbeat_t;

typedef struct{
    enum {
        ANT_PILL_DATA = 0,
        ANT_PILL_HEARTBEAT
    } type;
    uint8_t version;
    uint8_t reserved[2];
    uint64_t UUID;
    //uint64_t time;  // Morpheus should attch time, pill don't keep track of time anymore.
    union {
        uint32_t data[TF_CONDENSED_BUFFER_SIZE];
        pill_heartbeat_t heartbeat_data;
    } payload;
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

#define ANT_ChannelID_t hlo_ant_device_t
typedef struct{
    /* Called when a known and connected device sends a message */
    void (*on_message)(const hlo_ant_device_t * id, MSG_Address_t src, MSG_Data_t * msg);

    /* Called when an unknown device initiates advertisement */
    void (*on_unknown_device)(const hlo_ant_device_t * id);

    void (*on_status_update)(const hlo_ant_device_t * * id, ANT_Status_t status);
}MSG_ANTHandler_t;

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent, const MSG_ANTHandler_t * handler);
/* returns the number of connected devices */
uint8_t MSG_ANT_BondCount(void);
