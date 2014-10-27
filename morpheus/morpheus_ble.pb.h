/* Automatically generated nanopb header */
/* Generated by nanopb-0.2.8 at Mon Oct 27 01:25:03 2014. */

#ifndef _PB_MORPHEUS_BLE_PB_H_
#define _PB_MORPHEUS_BLE_PB_H_
#include <pb.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
typedef enum _ErrorType {
    ErrorType_TIME_OUT = 0,
    ErrorType_NETWORK_ERROR = 1,
    ErrorType_DEVICE_ALREADY_PAIRED = 2,
    ErrorType_INTERNAL_DATA_ERROR = 3,
    ErrorType_DEVICE_DATABASE_FULL = 4,
    ErrorType_DEVICE_NO_MEMORY = 5,
    ErrorType_INTERNAL_OPERATION_FAILED = 6,
    ErrorType_NO_ENDPOINT_IN_RANGE = 7,
    ErrorType_WLAN_CONNECTION_ERROR = 8,
    ErrorType_FAIL_TO_OBTAIN_IP = 9
} ErrorType;

typedef enum _MorpheusCommand_CommandType {
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_TIME = 0,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_TIME = 1,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_WIFI_ENDPOINT = 2,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_WIFI_ENDPOINT = 3,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_ALARMS = 4,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_ALARMS = 5,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE = 6,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE = 7,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_START_WIFISCAN = 8,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_STOP_WIFISCAN = 9,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID = 10,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_EREASE_PAIRED_PHONE = 11,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_PILL = 12,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_ERROR = 13,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_PAIR_SENSE = 14,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_UNPAIR_PILL = 15,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_MORPHEUS_DFU_BEGIN = 16,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DATA = 17,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_HEARTBEAT = 18,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_PILL_DFU_BEGIN = 19,
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET = 20
} MorpheusCommand_CommandType;

/* Struct definitions */
typedef struct _MorpheusCommand {
    int32_t version;
    MorpheusCommand_CommandType type;
    pb_callback_t deviceId;
    pb_callback_t accountId;
    bool has_error;
    ErrorType error;
    pb_callback_t wifiName;
    pb_callback_t wifiSSID;
    pb_callback_t wifiPassword;
    bool has_batteryLevel;
    int32_t batteryLevel;
    bool has_uptime;
    int32_t uptime;
    bool has_motionData;
    int32_t motionData;
    pb_callback_t motionDataEntrypted;
    bool has_firmwareVersion;
    int32_t firmwareVersion;
} MorpheusCommand;

/* Default values for struct fields */

/* Field tags (for use in manual encoding/decoding) */
#define MorpheusCommand_version_tag              1
#define MorpheusCommand_type_tag                 2
#define MorpheusCommand_deviceId_tag             3
#define MorpheusCommand_accountId_tag            4
#define MorpheusCommand_error_tag                5
#define MorpheusCommand_wifiName_tag             6
#define MorpheusCommand_wifiSSID_tag             7
#define MorpheusCommand_wifiPassword_tag         8
#define MorpheusCommand_batteryLevel_tag         9
#define MorpheusCommand_uptime_tag               10
#define MorpheusCommand_motionData_tag           11
#define MorpheusCommand_motionDataEntrypted_tag  12
#define MorpheusCommand_firmwareVersion_tag      13

/* Struct field encoding specification for nanopb */
extern const pb_field_t MorpheusCommand_fields[14];

/* Maximum encoded size of messages (where known) */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
