#include "message_led.h"
#include "led.h"
#include "led_booster_timer.h"
#include "util.h"
typedef struct{
    uint32_t time;
    int16_t rgb[3];
    int16_t valid;
}animation_node_t;

static struct{
    MSG_Base_t base;
    const MSG_Central_t * parent;
    uint32_t counter;
    enum MSG_LEDAddress animation;
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
static int
_play_test(int * out_r, int * out_g, int * out_b){
    static const animation_node_t seq[] = {
        {0    * BOOSTER_REFRESH_RATE, {0x18, 0x18, 0x18}, 1},
        {3    * BOOSTER_REFRESH_RATE, {0xff, 0xff, 0xff}, 0},
    };
    int i;
    const animation_node_t * current;
    for(i = 0; i < sizeof(seq)/sizeof(seq[0]); i++){
        if(self.counter >= seq[i].time){
            *out_r = seq[i].rgb[0];
            *out_g = seq[i].rgb[1];
            *out_b = seq[i].rgb[2];
            current = &seq[i];
        }
    }
    self.counter++;
    if(current){
        return current->valid;
    }
    return 0;
}
static int
_play_enter_factory_mode(int * out_r, int * out_g, int * out_b){
    static const animation_node_t seq[] = {
        {0    * BOOSTER_REFRESH_RATE, {0x18, 0x18, 0x18}, 1},
        {0.25 * BOOSTER_REFRESH_RATE, {0xff, 0xff, 0xff}, 1},
        {0.5  * BOOSTER_REFRESH_RATE, {0x18, 0x18, 0x18}, 1},
        {0.75 * BOOSTER_REFRESH_RATE, {0xff, 0xff, 0xff}, 0},
    };
    int i;
    const animation_node_t * current;
    for(i = 0; i < sizeof(seq)/sizeof(seq[0]); i++){
        if(self.counter >= seq[i].time){
            *out_r = seq[i].rgb[0];
            *out_g = seq[i].rgb[1];
            *out_b = seq[i].rgb[2];
            current = &seq[i];
        }
    }
    self.counter++;
    if(current){
        return current->valid;
    }
    return 0;
}
static int
_play_on(int * out_r, int * out_g, int * out_b){
    *out_r = 0x18;
    *out_g = 0x18;
    *out_b = 0x18;
    return 1;
}
static int
_play_boot_complete(int * out_r, int * out_g, int * out_b){
    static const animation_node_t seq[] = {
        {0   * BOOSTER_REFRESH_RATE, {0xff, 0x18, 0xff}, 1},
        {0.5 * BOOSTER_REFRESH_RATE, {0xff, 0xff, 0xff}, 1},
        {1   * BOOSTER_REFRESH_RATE, {0xff, 0x18, 0xff}, 1},
        {1.5 * BOOSTER_REFRESH_RATE, {0xff, 0xff, 0xff}, 1},
        {2   * BOOSTER_REFRESH_RATE, {0xff, 0x18, 0xff}, 1},
        {2.5 * BOOSTER_REFRESH_RATE, {0xff, 0xff, 0xff}, 0},
    };
    int i;
    const animation_node_t * current;
    for(i = 0; i < sizeof(seq)/sizeof(seq[0]); i++){
        if(self.counter >= seq[i].time){
            *out_r = seq[i].rgb[0];
            *out_g = seq[i].rgb[1];
            *out_b = seq[i].rgb[2];
            current = &seq[i];
        }
    }
    self.counter++;
    if(current){
        return current->valid;
    }
    return 0;
}
static int _on_cycle(int * out_r, int * out_g, int * out_b){
    /*
     *PRINTS("cycle\r\n");
     */
    switch(self.animation){
        default:
            return 0;
        case LED_PLAY_BOOT_COMPLETE:
            return _play_boot_complete(out_r, out_g, out_b);
        case LED_PLAY_ENTER_FACTORY_MODE:
            return _play_enter_factory_mode(out_r, out_g, out_b);
        case LED_PLAY_TEST:
            return _play_test(out_r, out_g, out_b);
        case LED_PLAY_ON:
            return _play_on(out_r, out_g, out_b);
    }
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

static int
_play_animation(enum MSG_LEDAddress type){
    if(led_booster_is_free()){
        self.counter = 0;
        led_booster_power_on();
        self.animation = type;
        return 0;
    }else{
        led_booster_power_off();
        return -1;
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
    if(dst.submodule > 0 && dst.submodule < LED_COMMAND_SIZE){
        _play_animation(dst.submodule);
        return SUCCESS;
    }
    return FAIL;
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
