#include "boot_test.h"
#include "message_led.h"

#define ALL_OK (ANT_OK | IMU_OK)
void test_ok(const MSG_Central_t * parent, uint8_t mask){
    static uint16_t boot_state;
    boot_state |= mask;
    if(boot_state == ALL_OK){
        //all ok
        parent->dispatch((MSG_Address_t){CENTRAL,0}, (MSG_Address_t){LED,LED_PLAY_BOOT_COMPLETE}, NULL);
    }
}
