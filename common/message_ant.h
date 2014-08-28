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
    ANT_FUNCTION_NULL = 0,      /* ignore this packet */
    ANT_FUNCTION_TEST = 0x66,   /* receiver echos back the payload */
    ANT_FUNCTION_END = 0xFF
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

/**
 * action when it sees unknown device
 */
typedef enum{
    ANT_DISCOVERY_NO_ACTION = 0,
    ANT_DISCOVERY_REPORT_DEVICE,
    ANT_DISCOVERY_ACCEPT_NEXT_DEVICE,
    ANT_DISCOVERY_ACCEPT_ALL_DEVICE,
}ANT_DISCOVERY_STATE;

typedef struct{
    enum{
        ANT_PING=0,
        ANT_SET_ROLE,//sets device role
        ANT_CREATE_SESSION,
        ANT_ADVERTISE,
        ANT_SEND_RAW,
        ANT_END_CMD
    }cmd;
    union{
        ANT_DISCOVERY_ROLE role;
        ANT_Channel_Settings_t settings;
        ANT_ChannelID_t session_info;
        uint8_t raw_data[8];
    }param;
}MSG_ANTCommand_t;

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent);
/* returns the number of connected devices */
uint8_t MSG_ANT_BondCount(void);
