/* Linker script to configure memory regions. */
MEMORY
{
  SOFTDEVICE (rx) : ORIGIN = 0x0, LENGTH = 0x14000 /* Nordic SoftDevice code */
  APP (rx) : ORIGIN = 0x14000, LENGTH = 0x22000
  BOOTLOADER (rx) : ORIGIN = 0x36000, LENGTH = 0x8C00
  USER_DATA (rw) : ORIGIN = 0x3EC00, LENGTH = 0x400
  RAM (rwx) : ORIGIN = 0x20002000, LENGTH = 0x2000
}
