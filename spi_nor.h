#pragma once

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

