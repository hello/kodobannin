/* Linker script to configure memory regions.
 *  CODE PAGE SIZE in FICR for nrf514 is 0x400
 *  CODE SIZE is 0x100
 *  FOr APP and APP Data, make sure they are aligned with multiple of CODE PAGE SIZE or N * 0x400
 */
MEMORY
{
  SOFTDEVICE (rx) : ORIGIN = 0x0, LENGTH = 0x20000 /* 0x80 pages */
  APP (rx) : ORIGIN = 0x20000, LENGTH = 0x16000 /* 0x58 Pages */
  /* DFU_APP_DATA_RESERVED _can_ occupy space here if we wish; this
     may be a better idea than defining our own USER_DATA section,
     since it's SDK-supported. */
  BOOTLOADER (rx) : ORIGIN = 0x36000, LENGTH = 0x7000 /*  0x1C pages */
  /* App Persistent Storage */
  APP_DATA (rw) : ORIGIN = 0x3D000, LENGTH = 0x2000 /* 0x8 pages */
  /* Bootloader storage */
  USER_DATA (rw) : ORIGIN = 0x3F000, LENGTH = 0x1000 /* 0x5 pages */
  /* Make sure the total of the above are  of the TOTAL 100 pages */
  SOFTDEVICE_RAM (rwx): ORIGIN = 0x20000000, LENGTH = 0x2400
  RAM (rwx) : ORIGIN = 0x20002400, LENGTH = 0x1BF0
  KEYSTORE (rw) : ORIGIN = 0x20003FF0, LENGTH = 0x10 /* place where the identity information are stored*/
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
