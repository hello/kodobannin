#include <spi.h>
#include <spi_nor.h>
#include <stdlib.h>
#include <util.h>

static enum SPI_Channel _chan = SPI_Channel_Invalid;
static uint32_t _nCS;

static NOR_Chip_Config _nor_configs[] = {
	NOR_CHIP(NOR_Mfg_Macronix, NOR_Chip_MX25U128, NOR_CAPACITY_16M, NOR_BLOCK_SIZE_4K, NOR_PAGE_SIZE_256), // Macronix MX25U128
	NOR_CHIP(NOR_Mfg_Macronix, NOR_Chip_MX25U256, NOR_CAPACITY_32M, NOR_BLOCK_SIZE_4K, NOR_PAGE_SIZE_256), // Macronix MX25U256
};

static const uint32_t _nor_config_count = sizeof(_nor_configs)/sizeof(NOR_Chip_Config);
static NOR_Chip_Config *_nor_config;

static bool
_read_status(uint8_t *status) {
	uint8_t data;
	int32_t ret;

	data = SPI_Read(CMD_RDSR);

    ret = spi_xfer2(_chan, _nCS, 1, &data, 1, &data);
    
    *status = data;
    
    return ret == 1;
}

static bool
_read_id(uint8_t *mfg_id, uint8_t *chip_id) {
	uint8_t data[4];
	int32_t ret;

	data[0] = SPI_Read(CMD_REMS);
	data[1] = 0xFF;
	data[2] = 0xFF;
	data[3] = 0;

	ret = spi_xfer2(_chan, _nCS, 4, data, 2, data);    

    *mfg_id  = data[0];
    *chip_id = data[1];

    return ret == 2;
}

static NOR_Chip_Config *
_find_nor_config(uint8_t mfg_id, uint8_t chip_id) {
	int i;

	for (i = 0; i < _nor_config_count; i++) {
		if (_nor_configs[0].vendor_id == mfg_id && _nor_configs[i].chip_id == chip_id) {
			return &_nor_configs[i];
		}
	}

	return NULL;
}

uint32_t
spinor_init(enum SPI_Channel chan, enum SPI_Mode mode, uint32_t miso, uint32_t mosi, uint32_t sclk, uint32_t nCS) {
	uint32_t err;
	uint8_t mfg_id;
	uint8_t chip_id;

	_nCS = nCS;
	_chan = chan;

	err = spi_init(chan, mode, miso, mosi, sclk, nCS);
	if (err != 0) {
		PRINTS("Could not configure SPI bus for NOR\r\n");
		return err;
	}

	if (!_read_id(&mfg_id, &chip_id)) {
		PRINTS("Could not query NOR identity\r\n");
		return -3;
	}

	_nor_config = _find_nor_config(mfg_id, chip_id);
	if (!_nor_config) {
		PRINTS("Could not find NOR chip config\r\n");
		return -4;
	}

	PRINTS("NOR MFG/CHIP ID: ");
	PRINT_HEX(&_nor_config->vendor_id, 1);
	PRINTS("/ ");
	PRINT_HEX(&_nor_config->chip_id, 1);

	PRINTS("\r\nCapacity: 0x");
	uint8_t temp = (uint8_t)(_nor_config->capacity / 1024 / 1024);
	PRINT_HEX(&temp, 1);
	PRINTS("MiB\r\n");
		
	return 0;
}

int32_t
spinor_read(uint32_t address, uint16_t len, uint8_t *buffer) {
	uint8_t data[4];

	data[0] = SPI_Read(CMD_READ);
	data[1] = address & 0xFF;
	data[2] = (address >> 8) & 0xFF;
	data[3] = (address >> 16) & 0xFF;

	return spi_xfer2(_chan, _nCS, 4, data, len, buffer); 
}