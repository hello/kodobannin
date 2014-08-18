#pragma once
/**
 * This file defines the Hello Network of ANT devices
 */


/**
 * Channel_ID -> Device_Type
 * Indicates the type of hardware 
 */
#define HLO_ANT_DEVICE_TYPE_PILL_BASE  (0x10)
#define HLO_ANT_DEVICE_TYPE_SENSE_BASE (0x20)

#define HLO_ANT_DEVICE_TYPE_PILL_EVT (HLO_ANT_DEVICE_TYPE_PILL_BASE + 1)
#define HLO_ANT_DEVICE_TYPE_SENSE_EVT (HLO_ANT_DEVICE_TYPE_SENSE_BASE + 1)


/**
 * HLO Network Key
 * TODO Mock Key here
 */
#define HLO_ANT_NETWORK_KEY {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77}
#define HLO_ANT_NETWORK_FREQ 0x31
