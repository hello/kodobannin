#pragma once
#include "message_base.h"

#define ANT_DISCOVERY_CHANNEL 0

//how often does each payload transmit
#define ANT_DEFAULT_TRANSMIT_LIMIT 3

//how often header transmits before the rest of payload
#define ANT_DEFAULT_HEADER_TRANSMIT_LIMIT 5

/**
 *
 * HLO ANT Air Protocol Constraints v0.1
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
 * |              Single Frame Packets                  |
 * +----------------------------------------------------+
 * |   page   |page_count|function                      |
 * +----------------------------------------------------+
 * |    0     |     0    |null function packet(ignore)  |
 * |  n > 0   |     0    |See ANT_FunctionType_t        |
 * |          |     0    |                              |
 * +----------------------------------------------------+
 **/

typedef enum{
    /* Reserved Mask Range 0x00 - 0x0F */
    ANT_FUNCTION_NULL = 0,              /* ignore this packet */
    /* Discovery Mask Range 0x10 - 0x1F */
    /* Discovery Mask is only mask that is handled without an established session */
    ANT_FUNCTION_DISC_UUID = 0x10,          /* UUID broadcast */
    ANT_FUNCTION_DISC_TYPE = 0x11,          /* Device Type broadcast */
    ANT_FUNCTION_DISC_SW_VERSION = 0x12,    /* SW Version */
    ANT_FUNCTION_DISC_HW_VERSION = 0x13,    /* HW Version */
    /* Test functions range 0x20 - 0x2F */
    ANT_FUNCTION_TEST = 0x20,               /* receiver echos back the payload */
    /* Control functions range 0x80 - 0x8F */
    ANT_FUNCTION_SET_TIME = 0x80            /* set 32bit unix time */
}ANT_FunctionType_t;

typedef struct{
    uint8_t page;
    uint8_t page_count;
    uint8_t reserved0;
    uint8_t reserved1;
    uint16_t size;
    uint16_t checksum;
}ANT_HeaderPacket_t;

typedef struct{
    uint8_t page;
    uint8_t page_count;//non 0 if it's a MSG_Data
    uint8_t payload[6];//unused payload must be set to 0
}ANT_PayloadPacket_t;


/* Do not modify member arrangement */
typedef struct{
    uint16_t device_number;
    uint8_t device_type;
    uint8_t transmit_type;
}ANT_ChannelID_t;


typedef struct{
    //cached status
    uint8_t reserved;
    //MASTER/SLAVE/SHARED etc
    uint8_t channel_type;
    //2.4GHz + frequency, 2.4XX
    uint8_t frequency;
    //network key, HLO specific
    uint8_t network;
    //period 32768/period Hz
    uint16_t period;
}ANT_ChannelPHY_t;

/**
 * Discovery profile
 * Discovery profiles are single page data packets
 * They are not assembled by the central, but rather are used as information for pairing
 */
typedef struct{
    uint8_t reserved[8];
}ANT_DiscoveryProfile_t;


typedef struct{
    ANT_ChannelPHY_t phy;
    ANT_ChannelID_t id;
}ANT_Channel_Settings_t;

/**
 * device roles
 */
typedef enum{
    ANT_DISCOVERY_CENTRAL = 0,
    ANT_DISCOVERY_PERIPHERAL
}ANT_DISCOVERY_ROLE;

typedef struct{
    enum{
        ANT_PING=0,
        ANT_SET_ROLE,//sets device role
        ANT_CREATE_SESSION,
        ANT_ADVERTISE,
        ANT_ADVERTISE_STOP,
        ANT_SEND_RAW,
        ANT_END_CMD,
        ANT_REMOVE_DEVICE,
        ANT_ACK_DEVICE_REMOVED,
        ANT_ACK_DEVICE_PAIRED
    }cmd;
    union{
        ANT_DISCOVERY_ROLE role;
        ANT_Channel_Settings_t settings;
        ANT_ChannelID_t session_info;
        uint8_t raw_data[8];
    }param;
}MSG_ANTCommand_t;

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
    void (*on_message)(const ANT_ChannelID_t * id, MSG_Address_t src, MSG_Data_t * msg);

    /* Called when an unknown device initiates advertisement */
    void (*on_unknown_device)(const ANT_ChannelID_t * id);

    /* Called when a known and connected device sends a single frame message */
    void (*on_control_message)(const ANT_ChannelID_t * id, MSG_Address_t src, uint8_t control_type, const uint8_t * control_payload);

    void (*on_status_update)(const ANT_ChannelID_t * id, ANT_Status_t status);
}MSG_ANTHandler_t;

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent, const MSG_ANTHandler_t * handler);
/* returns the number of connected devices */
uint8_t MSG_ANT_BondCount(void);
