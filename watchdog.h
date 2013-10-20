#include <nrf51.h>
#include <nrf51_bitfields.h>

/// Reset the watchdog timer.

#include <nrf51.h>
#include <nrf51_bitfields.h>

/// Call watchdog_pet() to reset the watchdog timer.
#define watchdog_pet() do { NRF_WDT->RR[0] = WDT_RR_RR_Reload; } while (0)

/// Initialize and start the watchdog timer (WDT), with the timeout specified in seconds. If the WDT doesn't receive a heartbeat from the application via watchdog_pet(), the system will reboot. THE DEBUG INTERFACE WILL BE DISABLED AFTER THIS FUNCTION IS CALLED.
void watchdog_init(unsigned seconds, bool pauseTimerWhileCPUSleeping);
