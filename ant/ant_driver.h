#pragma once
#include <stdint.h>
#include <stdbool.h>
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
    uint8_t measurement_type;
    int8_t rssi;
}hlo_ant_device_t;

typedef struct{
    /* tx event indicates opportunity to send data to the id */
    bool (*on_tx_event)(const hlo_ant_device_t * device, uint8_t * out_buffer, hlo_ant_role role, bool lockstep);
    /* rx event indicates receiving a packet from the channel */
    /* @param   device      the remote device type
     * @param   buffer      the buffer sent by the remote device
     * @param   buffer_len  the size of the said buffer
     * @param   role        the role of the local device (central/peripheral)
     * @param   ack         central only: ack back the said remote device, default is true unless overridden(for legacy pills)
     */
    void (*on_rx_event)(const hlo_ant_device_t * device, uint8_t * buffer, uint8_t buffer_len, hlo_ant_role role, bool * ack);
    void (*on_error_event)(const hlo_ant_device_t * device, uint32_t event);
}hlo_ant_event_listener_t;

int32_t hlo_ant_init(hlo_ant_role role, const hlo_ant_event_listener_t * callbacks);
int32_t hlo_ant_connect(const hlo_ant_device_t * device, bool full_duplex);
int32_t hlo_ant_disconnect(const hlo_ant_device_t * device);
int32_t hlo_ant_pause_radio(void);
int32_t hlo_ant_resume_radio(void);
int32_t hlo_ant_cw_test(uint8_t freq, uint8_t tx_power);


