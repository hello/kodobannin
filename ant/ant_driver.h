#pragma once
#include <stdint.h>
typedef enum{
    HLO_ANT_ROLE_CENTRAL,
    HLO_ANT_ROLE_PERIPHERAL
}hlo_ant_role;

/*
 * object for an ant device
 * uniquely identifies the device
 */
typedef struct{
    uint16_t device_number;
    uint8_t device_type;
    uint8_t transmit_type;
}hlo_ant_device_t;

typedef struct{
    /* tx event indicates opportunity to send data to the id */
    void (*on_tx_event)(const hlo_ant_device_t * device, uint8_t * out_buffer, uint8_t * out_buffer_len);
    /* rx event indicates receiving a packet from the channel */
    void (*on_rx_event)(const hlo_ant_device_t * device, uint8_t * buffer, uint8_t buffer_len);
    void (*on_error_event)(const hlo_ant_device_t * device, uint32_t event);
}hlo_ant_event_listener_t;

int32_t hlo_ant_init(hlo_ant_role role, const hlo_ant_event_listener_t * callbacks);
int32_t hlo_ant_connect(const hlo_ant_device_t * device);
int32_t hlo_ant_disconnect(const hlo_ant_device_t * device);
int32_t hlo_ant_pause_radio(void);
int32_t hlo_ant_resume_radio(void);


