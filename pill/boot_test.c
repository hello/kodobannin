#include "boot_test.h"
#include "message_led.h"

static boot_state;

void test_ok(const MSG_Central_t * parent, uint8_t mask){
    boot_state |= mask;
    if(boot_state == ALL_OK){
        //all ok
        parent->dispatch((MSG_Address_t){CENTRAL,0}, (MSG_Address_t){LED,LED_PLAY_BOOT_COMPLETE}, NULL);
    }
}
