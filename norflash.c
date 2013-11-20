#include <spi.h>
#include <norflash.h>
#include <app_error.h>

NOR_Chip_Config supported_chips[] = {
	// Macronix MX25U128
	NOR_CHIP(0xFE, 0xED, NOR_CAPACITY_16M, NOR_BLOCK_SIZE_4K, NOR_PAGE_SIZE_512),
};

static NOR_Chip_Config *_config;
