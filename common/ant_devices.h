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

#define HLO_ANT_DEVICE_TYPE_PILL      (HLO_ANT_DEVICE_TYPE_PILL_BASE + 1)
#define HLO_ANT_DEVICE_TYPE_SENSE     (HLO_ANT_DEVICE_TYPE_SENSE_BASE + 1)
