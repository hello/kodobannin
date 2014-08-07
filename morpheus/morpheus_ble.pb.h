/* Automatically generated nanopb header */
/* Generated by nanopb-0.2.8 at Thu Aug  7 11:28:52 2014. */

#ifndef _PB_MORPHEUS_BLE_PB_H_
#define _PB_MORPHEUS_BLE_PB_H_
#include <pb.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
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
    MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID = 10
} MorpheusCommand_CommandType;

/* Struct definitions */
typedef struct _WifiEndPoint {
    pb_callback_t name;
    pb_callback_t mac;
} WifiEndPoint;

typedef struct _SelectedWifiEndPoint {
    bool has_endPoint;
    WifiEndPoint endPoint;
    pb_callback_t password;
} SelectedWifiEndPoint;

typedef struct _MorpheusCommand {
    int32_t version;
    MorpheusCommand_CommandType type;
    bool has_selectedWIFIEndPoint;
    SelectedWifiEndPoint selectedWIFIEndPoint;
} MorpheusCommand;

/* Default values for struct fields */

/* Field tags (for use in manual encoding/decoding) */
#define WifiEndPoint_name_tag                    1
#define WifiEndPoint_mac_tag                     2
#define SelectedWifiEndPoint_endPoint_tag        1
#define SelectedWifiEndPoint_password_tag        2
#define MorpheusCommand_version_tag              1
#define MorpheusCommand_type_tag                 2
#define MorpheusCommand_selectedWIFIEndPoint_tag 3

/* Struct field encoding specification for nanopb */
extern const pb_field_t WifiEndPoint_fields[3];
extern const pb_field_t SelectedWifiEndPoint_fields[3];
extern const pb_field_t MorpheusCommand_fields[4];

/* Maximum encoded size of messages (where known) */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
