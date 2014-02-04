// vi:sw=4:ts=4

#include <drivers/spi.h>
#include <drivers/spi_nor.h>
#include <stdlib.h>
#include <util.h>
#include <app_error.h>
#include <string.h>

static SPI_Context _ctx;

static NOR_Chip_Config _nor_configs[] = {
	NOR_CHIP(NOR_Mfg_Macronix, NOR_Chip_MX25U128, NOR_CAPACITY_16M, NOR_BLOCK_SIZE_4K, NOR_PAGE_SIZE_256), // Macronix MX25U128
	NOR_CHIP(NOR_Mfg_Macronix, NOR_Chip_MX25U256, NOR_CAPACITY_32M, NOR_BLOCK_SIZE_4K, NOR_PAGE_SIZE_256), // Macronix MX25U256
};

static const uint32_t _nor_config_count = sizeof(_nor_configs)/sizeof(NOR_Chip_Config);
static NOR_Chip_Config *_nor_config = NULL;


static bool _in_secure_mode = false;

static bool
_read_status(uint8_t *status) {
	uint8_t data;
	int32_t ret;

	data = CMD_RDSR;

    ret = spi_xfer(&_ctx, 1, &data, 1, status);

    return ret == 1;
}

static bool
_read_scur(uint8_t *status) {
	uint8_t data;
	int32_t ret;

	data = CMD_RDSCUR;

	ret = spi_xfer(&_ctx, 1, &data, 1, status);

	return ret == 1;
}

static bool
_read_id(uint8_t *mfg_id, uint8_t *chip_id) {
	uint8_t data[4];
	int32_t ret;

	data[0] = CMD_REMS;
	data[1] = 0xFF;
	data[2] = 0xFF;
	data[3] = 0;

	ret = spi_xfer(&_ctx, 4, data, 2, data);
//	DEBUG("IDS: ", *data);
    *mfg_id  = data[0];
    *chip_id = data[1];

    return ret == 2;
}

static inline uint8_t
_RDSR_wait() {
	uint8_t data;
	uint32_t count = 0;

	do {
		if (!_read_status(&data)) {
			return -2;
		}
	} while ((data & NOR_RDSR_WIP) && ++count < 0xFFFF);

	//DEBUG("RDSR: ", data);
	//DEBUG("Count: ", count);

	return data;
}

static int32_t
_write_enable() {
	uint8_t data = CMD_WREN;
	uint8_t rdsr;

	rdsr = _RDSR_wait();

	if (spi_xfer(&_ctx, 1, &data, 0, NULL) != 1) {
		return -1;
	}

	rdsr = _RDSR_wait();


	// ensure the chip acknoledges the write enable
	if (!(rdsr & NOR_RDSR_WEL)) {
		return -3;
	}

	return 0;
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

int32_t
spinor_init(enum SPI_Channel chan, enum SPI_Mode mode, uint32_t miso, uint32_t mosi, uint32_t sclk, uint32_t nCS) {
	uint32_t err;
	uint8_t mfg_id;
	uint8_t chip_id;

	err = spi_init(chan, mode, miso, mosi, sclk, nCS, &_ctx);
	if (err != 0) {
		//PRINTS("Could not configure SPI bus for NOR\r\n");
		return err;
	}

	if (!_read_id(&mfg_id, &chip_id)) {
		//PRINTS("Could not query NOR identity\r\n");
		return -3;
	}

	_nor_config = _find_nor_config(mfg_id, chip_id);
	if (!_nor_config) {
		PRINTS("Could not find NOR chip config\r\n");
		DEBUG("MFG: ", mfg_id);
		DEBUG("CHIP: ", chip_id);
		return -4;
	}
/*
	PRINTS("NOR MFG/CHIP ID: ");
	PRINT_HEX(&_nor_config->vendor_id, 1);
	PRINTS("/ ");
	PRINT_HEX(&_nor_config->chip_id, 1);

	PRINTS("\r\nCapacity: 0x");
	uint8_t temp = (uint8_t)(_nor_config->capacity / 1024 / 1024);
	PRINT_HEX(&temp, 1);
	PRINTS("MiB\r\n");
*/
	return 0;
}

int32_t
spinor_enter_secure_mode() {
	uint8_t data[4] = {CMD_ENSO,0,0,0};
	int32_t ret;

	ret = spi_xfer(&_ctx, 1, data, 0, NULL);

	if (ret >0)
		_in_secure_mode = true;

	return ret;
}

int32_t
spinor_exit_secure_mode() {
	uint8_t data[4] = {CMD_EXSO,0,0,0};
	int32_t ret;

	ret = spi_xfer(&_ctx, 1, data, 0, NULL);

	if (ret >0)
		_in_secure_mode = false;

	return ret;
}

int32_t
spinor_in_secure_mode() {
	return _in_secure_mode;
}

int32_t
spinor_read(uint32_t address, uint32_t len, uint8_t *buffer) {
	uint8_t data[4];

	data[0] = (CMD_READ);
	data[1] = address & 0xFF;
	data[2] = (address >> 8) & 0xFF;
	data[3] = (address >> 16) & 0xFF;

	return spi_xfer(&_ctx, 4, data, len, buffer);
}

int32_t
spinor_write_page(uint32_t address, uint32_t len, uint8_t *buffer) {
	uint8_t data[4];
	int32_t err;

	// disallow writes for more than page_size bytes
	if (len > _nor_config->page_size)
		return -1;

	err = _write_enable();
	if (err != 0)
		return err;

	if (address & 0xFF000000) {
		PRINTS("Address for write is too large");
		return -2;
	}

	data[0] = (CMD_PP);
	data[1] = (address >> 16) & 0xFF;
	data[2] = (address >>  8) & 0xFF;
	data[3] = address & 0xFF;

	//DEBUG("Writing n bytes: 0x", len);
	//PRINT_HEX(buffer, len);
	//PRINTS("\r\n");

	err = spi_command(&_ctx, 4, data, len, buffer, 0, NULL);
	//DEBUG("Wrote ", err);

	if (err < 0)
		return err;

	return len;
}

int32_t
spinor_write(uint32_t address, uint32_t len, uint8_t *buffer)
{
	int32_t ret;
	uint32_t to_write;
	uint32_t written = 0;

	while (written < len) {
		to_write = len - written;

		// cap write size at NOR page size
		if (to_write > _nor_config->page_size)
			to_write = _nor_config->page_size;

		// cap write size to account for where address starts in the page
		if (address & 0xFF) {
			uint32_t spare = _nor_config->page_size - (address & 0xFF);
			if (to_write > spare)
				to_write = spare;
		}

		ret = spinor_write_page(address, to_write, buffer);

		if (ret < 0)
			return ret;

		address += ret;
		buffer += ret;
		written += ret;
	}

	return written;
}

int32_t
spinor_chip_erase() {
	uint8_t data;
	int32_t err;

	err = _write_enable();
	if (err != 0)
		return err;

	data = CMD_CE;

	err = spi_xfer(&_ctx, 1, &data, 0, NULL);

	return err < 0 ? err : 0;
}

int32_t
spinor_block_erase(uint16_t block) {
	uint8_t data[4];
	int32_t err;

	if (block >= _nor_config->total_blocks) {
		PRINTS("Cannot erase invalid block number");
		return -1;
	}

	err = _write_enable();
	if (err != 0)
		return err;

	data[0] = CMD_SE;
	data[1] = (block >> 8) & 0xFF;
	data[2] = (block & 0xFF);
	data[3] = 0;

	err = spi_xfer(&_ctx, 4, data, 0, NULL);

	return err < 0 ? err : 0;
}

void
spinor_wait_completion() {
	bool ret;
	uint8_t status;
	uint32_t count = 0;
	do {
		ret = _read_status(&status);
		APP_ERROR_CHECK(ret != 1);
		++count;
		// might want a sleep here
	} while (status & NOR_RDSR_WIP);
	DEBUG("Completion spun: 0x", count);
}

NOR_Chip_Config *
spinor_get_chip_config() {
	return _nor_config;
}

int32_t
spinor_get_serial(uint8_t *serial) {
	int32_t ret = 0;

	ret = spinor_enter_secure_mode();
	if (ret < 0)
		return ret;

	ret = spinor_read(0, 24, serial);
	if (ret < 0) {
		int32_t ret2 = spinor_exit_secure_mode();
		if (ret2 < 0) {
			PRINTS("Could not exit secure spinor mode. Panic\r\n");
			APP_ERROR_CHECK_BOOL(0);
		}
		return ret;
	}

	ret = spinor_exit_secure_mode();
	return ret;
}

