#include <nrf51.h>
#include <nrf51_bitfields.h>

/// Reset the watchdog timer.

#include <nrf51.h>
#include <nrf51_bitfields.h>

/// Call watchdog_pet() to reset the watchdog timer.
#define watchdog_pet() do { if(NRF_WDT->RUNSTATUS){NRF_WDT->RR[0] = WDT_RR_RR_Reload;} } while (0)

/// Initialize and start the watchdog timer (WDT), with the timeout specified in seconds. If the WDT doesn't receive a heartbeat from the application via watchdog_pet(), the system will reboot. THE DEBUG INTERFACE WILL BE DISABLED AFTER THIS FUNCTION IS CALLED.
void watchdog_init(unsigned seconds, bool pauseTimerWhileCPUSleeping);

//automiacally kick the watchdog
//call this after init
//seconds value should be shorter than the watchdog timer to account for task delays etc
void watchdog_task_start(unsigned int seconds);
