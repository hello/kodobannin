/* Linker script to configure memory regions. */
MEMORY
{
  SOFTDEVICE (rx) : ORIGIN = 0x0, LENGTH = 0x20000 /* ~128K */
  APP (rx) : ORIGIN = 0x20000, LENGTH = 0x14000 /* ~73K */
  /* Persistent Storage */
  APP_DATA (rw) : ORIGIN = 0x34000, LENGTH = 0x2000 /* 16K */
  /* DFU_APP_DATA_RESERVED _can_ occupy space here if we wish; this
     may be a better idea than defining our own USER_DATA section,
     since it's SDK-supported. */
  BOOTLOADER (rx) : ORIGIN = 0x36000, LENGTH = 0x8C00 /* ~35K */
  USER_DATA (rw) : ORIGIN = 0x3EC00, LENGTH = 0x1400
  SOFTDEVICE_RAM (rwx): ORIGIN = 0x20000000, LENGTH = 0x2400
  RAM (rwx) : ORIGIN = 0x20002400, LENGTH = 0x1C00
}

/* Note that this is the standard nRF SDK DFU layout:

   SOFTDEVICE: 0x0     - 0x20000 (length 0x20000, ~128k)
   APP:        0x20000 - 0x3C800 (length 0x28800, ~164k, for single bank)
   APP1:       0x20000 - 0x28400 (length 0x14400, ~80k, for dual bank)
   APP2:       0x28400 - 0x3C800 (length 0x14400, ~80k, for dual bank)
   app_data:   0x0 size
   BOOTLOADER: 0x3C800 - 0x40000 (length 0x3800,  ~14k)
   SHA1:       0x3F800
*/

/* [TODO]: Maybe move our user data to the DFU_APP_DATA_RESERVED
   area. */
