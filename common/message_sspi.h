#pragma once
#include <stddef.h>
#include "spi_slave.h"
#include "message_base.h"


typedef enum{
    MSG_SSPI_DEFAULT = 0,
    MSG_SSPI_MORPHEUS_BLE_PROTO = 1,
    MSG_SSPI_TEXT,
}MSG_SSPI_Ports;

MSG_Base_t * MSG_SSPI_Base(const spi_slave_config_t * p_spi_slave_config, const MSG_Central_t * parent);
