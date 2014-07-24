#pragma once
#include "spi_slave.h"
#include "message_base.h"


typedef struct{
    enum{
        SSPI_PING=0,
    }cmd;
    union{
    }param;
}MSG_SSPICommand_t;

MSG_Base_t * MSG_SSPI_Base(const spi_slave_config_t * p_spi_slave_config, const MSG_Central_t * parent);
