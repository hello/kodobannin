#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * factory provision module, 
 * do not call this with bootloader_start
 */

uint32_t factory_provision_start(void);

bool factory_needs_provisioning(void);
uint8_t * decrypt_key(void);




