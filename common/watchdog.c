// A lot of this code is based from <https://devzone.nordicsemi.com/index.php/watchdog-timer>.

/* Terminology:

   WDT = Watch Dog Timer.  Designed to reboot the system if it does
   not detect a heartbeat from the app within a certain amount of
   time.

   CRV = Counter reload value: number of clock cycles before the WDT
   reboots the system. You need to "pet" the watchdog by setting the
   Reload Request (RR) register to a specific values if you do not
   want the system to reboot.
 
   RR = Reload Request: A set of registers that, if written to, will
   prevent the WDT from rebooting the system.

   RREN = Reload Request Enable: A register that must be enabled for
   the WDT to acknowledge any Reload Requests (RRs).

   INTENSET = Interrupt Enable Set Register: 
*/

#include <app_util.h>

#include "watchdog.h"
#include "util.h"
#include "app.h"
#include <app_timer.h>

void watchdog_init(unsigned seconds, bool pauseTimerWhileCPUSleeping)
{
	NRF_WDT->CONFIG  = ((pauseTimerWhileCPUSleeping ? WDT_CONFIG_SLEEP_Pause : WDT_CONFIG_SLEEP_Run) << WDT_CONFIG_SLEEP_Pos);
    NRF_WDT->CONFIG |=  (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos);

	NRF_WDT->CRV = 32768 * seconds;
	NRF_WDT->RREN |= WDT_RREN_RR0_Msk;
	NRF_WDT->TASKS_START = 1;
}
static void _kick(void * ctx){
    PRINTS("Kick\r\n");
    if (NRF_WDT->RUNSTATUS == 1){
        PRINTS("running\r\n");
    }else{
        PRINTS("halted\r\n");
    }
    watchdog_pet();
}
void watchdog_task_start(unsigned int seconds){
    static app_timer_id_t timer;
    uint32_t ticks = APP_TIMER_TICKS(1000 * seconds, APP_TIMER_PRESCALER);
    if(NRF_SUCCESS == app_timer_create(&timer, APP_TIMER_MODE_REPEATED, _kick) && NRF_SUCCESS == app_timer_start(timer, ticks, NULL)){
        PRINTS("Watchdog task success\r\n");
    }else{
        //this is a critical failure, need to reboot to bootloader
    }
}
