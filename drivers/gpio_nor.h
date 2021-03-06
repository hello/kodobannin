#pragma once

#include "nrf_gpio.h"

void gpio_cfg_d0s1_output_disconnect(uint32_t pin_number);
void gpio_cfg_s0s1_output_connect(uint32_t pin_number, uint8_t init_val);
void gpio_input_disconnect(uint32_t pin_number);
void gpio_cfg_h0d1_opendrain_output_connect(uint32_t pin_number, uint8_t init_val, nrf_gpio_pin_pull_t pin_pull);
void gpio_cfg_d0s1_output_disconnect_pull(uint32_t pin_number, nrf_gpio_pin_pull_t pin_pull);
void gpio_cfg_d0s1_opendrain_output_connect(uint32_t pin_number, uint8_t init_val, nrf_gpio_pin_pull_t pin_pull);