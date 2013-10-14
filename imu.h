#pragma once
#include <spi.h>

uint32_t imu_init(enum SPI_Channel channel);
uint16_t imu_get_fifo_count();
uint16_t imu_fifo_read(uint16_t count, uint8_t *buf);
