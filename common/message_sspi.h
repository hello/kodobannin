#pragma once
#include <stddef.h>
#include "spi_slave.h"
#include "message_base.h"


MSG_Base_t * MSG_SSPI_Base(const spi_slave_config_t * p_spi_slave_config, const MSG_Central_t * parent);
