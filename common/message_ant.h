#pragma once
#include "message_base.h"

#define ANT_DISCOVERY_CHANNEL 0

//how often does each payload transmit
#define ANT_DEFAULT_TRANSMIT_LIMIT 3

//how often header packets are transmit between payload
//there are always one at the begining and one at the end
#define ANT_DEFAULT_HEADER_TRANSMIT_LIMIT 5

/**
 *
 * HLO ANT Air Protocol Constraints v0.1
 * 1. MetaData is always sent as page:0
 * 2. page count > 0
 * 3. crc32 is computed on receive of every meta page
 * 4. if meta page's crc changes, that means new message has arrived
 * 
 * 5. TX ends message by transmitting message N times (defined by user)
 * 6. RX ends message by either completing checksum, or channel closes
 *    - In case of master, receiving an invalid checksum will result in loss packet.  
 *    - Further protocol are user defined
 */

typedef struct{
    uint8_t page;
    uint8_t page_count;
    uint8_t reserved0;
    uint8_t reserved1;
    uint16_t size;
    uint16_t checksum;
}ANT_HeaderPacket_t;

typedef struct{
    uint8_t page;//always starts at 1, 
    uint8_t page_count;
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

typedef struct{
    enum{
        ANT_PING=0,
        ANT_SET_ROLE,//sets device role
        ANT_CREATE_SESSION,
        ANT_ADVERTISE,
        ANT_END_CMD
    }cmd;
    union{
        uint8_t role;
        ANT_Channel_Settings_t settings;
        ANT_ChannelID_t session_info;
    }param;
}MSG_ANTCommand_t;

MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent);
/* returns the number of connected devices */
uint8_t MSG_ANT_BondCount(void);
