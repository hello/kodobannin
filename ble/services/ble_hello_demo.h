/* Copyright (c) 2013 Hello Inc. All Rights Reserved. */

#pragma once

#include <stdint.h>
#include <ble_srv_common.h>
#include <ble.h>

#define BLE_UUID_HELLO_BASE {0x23, 0xD1, 0xBC, 0xEA, 0x5F, \
    0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00}

#define BLE_UUID_HELLO_DEMO_SVC 0x1337
#define BLE_UUID_DATA_CHAR      0xDA1A
#define BLE_UUID_CONF_CHAR      0xC0FF

typedef void (*ble_hello_demo_write_handler)(ble_gatts_evt_write_t *);

/**@brief Hello Demo init structure. This contains all possible characteristics 
 *        needed for initialization of the service.
 */
typedef struct
{
    ble_srv_security_mode_t        dis_attr_md;                 /**< Initial Security Setting for Hello Demo Characteristics. */
	ble_hello_demo_write_handler   data_write_handler;
	ble_hello_demo_write_handler   mode_write_handler;
} ble_hello_demo_init_t;

/**@brief Function for initializing the Hello Demo Service.
 *
 * @details This call allows the application to initialize the Hello Demo service. 
 *          It adds the service and characteristics to the database, using the initial
 *          values supplied through the p_init parameter. Characteristics which are not
 *          to be added, shall be set to NULL in p_init.
 *
 * @param[in]   p_init   The structure containing the values of characteristics needed by the
 *                       service.
 *
 * @return      NRF_SUCCESS on successful initialization of service.
 */
uint32_t ble_hello_demo_init(const ble_hello_demo_init_t * p_init);

void ble_hello_demo_on_ble_evt(ble_evt_t *event);
uint16_t ble_dts_data_send(const uint8_t * data, const uint16_t data_len);
