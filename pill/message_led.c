#include "message_led.h"
#include "led.h"
#include "led_booster_timer.h"
#include "util.h"
static struct{
    MSG_Base_t base;
    const MSG_Central_t * parent;
    uint32_t counter;
}self;
static const char * name = "LED";

static void _setup(void){
    /*
     *PRINTS("Setup\r\n");
     */
    led_power_on();
}
static void _teardown(void){
    /*
     *PRINTS("Teardown\r\n");
     */
    led_all_colors_off();
    led_power_off();
}
static void _on_warm(void){
    /*
     *PRINTS("Warm\r\n");
     */
    led_warm_up();
}
static int _on_cycle(int * out_r, int * out_g, int * out_b){
    static int led;
    /*
     *PRINTS("cycle\r\n");
     */
    if(self.counter++ > 30){
        return 0;
    }else{
        *out_r = 0x37;
        *out_g = 0x37;
        *out_b = 0x37;
    }
    return 1;
}

static MSG_Status
_init(void){
    led_booster_context_t ctx = (led_booster_context_t){
        .setup = _setup,
        .teardown = _teardown,
        .on_warm = _on_warm,
        .on_cycle = _on_cycle,
    };
    led_init();
    led_booster_init(&ctx);
    return SUCCESS;
}

void test_led(){
    if(led_booster_is_free()){
        self.counter = 0;
        led_booster_power_on();
    }else{
        led_booster_power_off();
    }
}
static MSG_Status
_destroy(void){
    return SUCCESS;
}
static MSG_Status
_flush(void){
    return SUCCESS;
}
static MSG_Status
_handle_led_commands(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    switch(dst.submodule){
        default:
            break;
        case LED_BLINK_GREEN:

            break;
    }
    return SUCCESS;
}

MSG_Base_t * MSG_LEDInit(MSG_Central_t * parent){
    self.base.init = _init;
    self.base.destroy = _destroy;
    self.base.flush = _flush;
    self.base.send = _handle_led_commands;
    self.base.type = LED;
    self.base.typestr = name;
    self.parent = parent;
    return &self.base;
}
