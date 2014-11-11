/* To force DFU on boot: NRF_POWER->GPREGRET |= FORCE_DFU_ON_BOOT_MASK
   To turn off the flag: NRF_POWER->GPREGRET &= ~FORCE_DFU_ON_BOOT_MASK
   To check the flag: NRF_POWER->GPREGRET & FORCE_DFU_ON_BOOT_MASK
*/

/// Mask for bit to force DFU on boot.
#define GPREGRET_FORCE_DFU_ON_BOOT_MASK ((uint32_t)0x01)

/// Mask to indicate that the app has crashed. You probably also want to force DFU on boot (via the GPREGRET_FORCE_DFU_ON_BOOT_MASK flag) if you set this flag.
#define GPREGRET_APP_CRASHED_MASK ((uint32_t)0x02)

/// Mask to boot app into dtm mode
#define GPREGRET_APP_BOOT_TO_DTM ((uint32_t)0x04)
