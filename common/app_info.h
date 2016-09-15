#pragma once
/* 
 * A place to put all common defines
 */
//make sure the FW_VERSION_STRING fits in MorpheusCommand.top_version (16 bytes)
#define FW_VERSION_STRING "1.5.7"

//pill only
#define FIRMWARE_VERSION_8BIT (57)

#define BLE_SIG_COMPANY_ID 1002

typedef struct __attribute__((packed)){
    uint8_t hw_type;    /* hw type, see ant_devices.h */
    uint8_t fw_version; /* version */
    uint64_t id;        /* device id */
}hlo_ble_adv_manuf_data_t ;

