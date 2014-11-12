#include "message_led.h"
#include "led.h"
#include "led_booster_timer.h"
#include "util.h"
static struct{
    MSG_Base_t base;
    const MSG_Central_t * parent;
    int counter;
}self;
static const char * name = "LED";

static void _setup(void){
    PRINTS("Setup\r\n");
    led_power_on();
}
static void _teardown(void){
    led_power_off();
}
static void _on_warm(void){
    PRINTS("Warm\r\n");
    led_all_colors_on();
    /*
     *led_flash(0, 1000, NULL); // cylce thru all three colors each time
     */
}
static int _on_cycle(led_booster_event_t event){
    PRINTS("cycle\r\n");
    if(self.counter++ > 10){
        return 0;
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
    led_booster_power_on();
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
