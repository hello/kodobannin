#pragma once

#include <spi.h>

enum SPI_NOR_Commands {
	CMD_NOP     = 0x00,
	CMD_WRSR    = 0x01, // Write Status / Config Register
	CMD_PP      = 0x02, // Page Program
	CMD_READ    = 0x03, // Read
	CMD_WRDI    = 0x04, // Write Disable
	CMD_RDSR    = 0x05, // Read Status Register
	CMD_WREN    = 0x06, // Write Enable
	CMD_FREAD   = 0x0B, // Fast Read
	CMD_PP4B    = 0x12, // Program Page with 4-byte address
	CMD_RDCR    = 0x15, // Read Config Register
	CMD_RDFBR   = 0x16, // Read Fast Boot Register
	CMD_WRFBR   = 0x17, // Write Fast Boot Register
	CMD_ESFBR   = 0x18, // Erase Fast Boot Register
	CMD_SE      = 0x20, // Sector Erase 4kb
	CMD_RDSCUR  = 0x2B, // Read Security Register
	CMD_WRSCUR  = 0x2F, // Write Security Register
	CMD_RESUME  = 0x30, // Program / Erase Resume
	CMD_BE32k   = 0x52, // Block Erase 32k
	CMD_RDSFDP  = 0x5A, // Read SFDP mode
	CMD_CE      = 0x60, // Chip Erase
	CMD_RSTEN   = 0x66, // Reset Enable
	CMD_WPSEL   = 0x68, // Write Protect Selection
	CMD_REMS    = 0x90, // Read Electronic Manufacturer & Device ID
	CMD_RDID    = 0x9F, // Read ID
	CMD_RST     = 0x99, // Reset
	CMD_PWRUP   = 0xAB, // Exit Deep power down
	CMD_SUSP    = 0xB0, // Program / Erase Suspend
	CMD_ENSO    = 0xB1, // Enter Secure OTP Mode
	CMD_PWRDP   = 0xB9, // Deep power down
	CMD_SBL     = 0xC0, // Set Burst Length
	CMD_EXSO    = 0xC1, // Exit Secure OTP Mode
	CMD_WREAR   = 0xC5, // Write Extended Address Register
	CMD_RDEAR   = 0xC8, // Read Extended Address Register
	CMD_BE64k   = 0xD8, // Block Erase 64k
};

enum NOR_RDSR_Bits {
	NOR_RDSR_WIP  = (1U << 0),
	NOR_RDSR_WEL  = (1U << 1),
	NOR_RDSR_BP0  = (1U << 2),
	NOR_RDSR_BP1  = (1U << 3),
	NOR_RDSR_BP2  = (1U << 4),
	NOR_RDSR_BP3  = (1U << 5),
	NOR_RDSR_QE   = (1U << 6),
	NOR_RDSR_SRWD = (1U << 7)
};

// Chip oriented constants and structures
#define NOR_CAPACITY_16M     (16 * 1024 * 1024)
#define NOR_CAPACITY_32M     (32 * 1024 * 1024)
#define NOR_PAGE_SIZE_256    (256)
#define NOR_PAGE_SIZE_512    (512)
#define NOR_BLOCK_SIZE_4K    (4096)

#define NOR_CHIP(vid, cid, cap, bs, ps) {vid, cid, cap, bs, ps, (cap/bs), (bs/ps)}

enum NOR_MfgID {
	NOR_Mfg_Macronix = 0xC2,
};

enum NOR_ChipID {
	NOR_Chip_MX25U128 = 0x38,
	NOR_Chip_MX25U256 = 0x39,
};

typedef struct {
	uint8_t  vendor_id;
	uint8_t  chip_id;
	uint32_t capacity;
	uint16_t block_size;
	uint16_t page_size;
	uint16_t total_blocks;
	uint16_t pages_per_block;
} NOR_Chip_Config;

uint32_t spinor_init(enum SPI_Channel chan, enum SPI_Mode mode, uint32_t miso, uint32_t mosi, uint32_t sclk, uint32_t nCS);
int32_t  spinor_read(uint32_t address, uint32_t len, uint8_t *buffer);
int32_t  spinor_write(uint32_t address, uint32_t len, uint8_t *buffer);
int32_t  spinor_write_page(uint32_t page_address, uint32_t len, uint8_t *buffer);
int32_t  spinor_chip_erase();
int32_t  spinor_block_erase(uint16_t block);
void     spinor_wait_completion();
