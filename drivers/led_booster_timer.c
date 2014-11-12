#include "led_booster_timer.h"
#include <app_timer.h>
#include "app.h"
//APP_TIMER_TICKS(x, APP_TIMER_PRESCALER)

typedef enum{
    LED_BOOSTER_STATE_COLD = 0,
    LED_BOOSTER_STATE_WARM,
    LED_BOOSTER_STATE_HOT
}led_booster_state_t;

static struct{
    volatile led_booster_state_t state;
    led_booster_context_t user;
    app_timer_id_t timer;
}self;

static void
_timer_handler(void * ctx){
    switch(self.state){
        default:
        case LED_BOOSTER_STATE_COLD:
            {
                self.user.teardown();
            }
            break;
        case LED_BOOSTER_STATE_WARM:
            {
                uint32_t ticks = APP_TIMER_TICKS(100, APP_TIMER_PRESCALER);
                self.state = LED_BOOSTER_STATE_HOT;
                self.user.on_warm();
                app_timer_start(self.timer, ticks, NULL);
            }
            break;
        case LED_BOOSTER_STATE_HOT:
            {
                uint32_t ticks = APP_TIMER_TICKS(100, APP_TIMER_PRESCALER);
                if(self.user.on_cycle(LED_BOOSTER_EVENT_START)){
                    app_timer_start(self.timer, ticks, NULL);
                }else{
                    self.state = LED_BOOSTER_STATE_COLD;
                }
            }
            break;
    }
}

void led_booster_init(const led_booster_context_t * ctx){
    self.user = *ctx;
    app_timer_create(&self.timer, APP_TIMER_MODE_SINGLE_SHOT, _timer_handler);
    self.state = LED_BOOSTER_STATE_COLD;
}

void led_booster_power_on(void){
    uint32_t ticks = APP_TIMER_TICKS(500, APP_TIMER_PRESCALER);
    app_timer_stop(self.timer);
    self.user.setup();
    self.state = LED_BOOSTER_STATE_WARM;
    app_timer_start(self.timer, ticks, NULL);
}
void led_booster_power_off(void){
    self.state = LED_BOOSTER_STATE_COLD;
}
