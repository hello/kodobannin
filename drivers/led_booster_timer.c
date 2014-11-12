#include "led_booster_timer.h"
#include <app_timer.h>
#include "app.h"
#include "led.h"
//APP_TIMER_TICKS(x, APP_TIMER_PRESCALER)

typedef enum{
    LED_BOOSTER_STATE_COLD = 0,
    LED_BOOSTER_STATE_WARM,
    LED_BOOSTER_STATE_HOT
}led_booster_state_t;

typedef enum{
    LED_BOOSTER_EVENT_START = 0,
    LED_BOOSTER_EVENT_BLUE,
    LED_BOOSTER_EVENT_GREEN,
    LED_BOOSTER_EVENT_RED
}led_booster_event_t;

static struct{
    volatile led_booster_state_t state;
    led_booster_context_t user;
    app_timer_id_t timer;
    int rgb[3];
    led_booster_event_t cycle_state;
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
                uint32_t ticks = APP_TIMER_TICKS(500, APP_TIMER_PRESCALER);
                self.state = LED_BOOSTER_STATE_HOT;
                self.user.on_warm();
                self.cycle_state = LED_BOOSTER_EVENT_START;
                app_timer_start(self.timer, ticks, NULL);
            }
            break;
        case LED_BOOSTER_STATE_HOT:
            {
                uint32_t ticks = APP_TIMER_TICKS(8, APP_TIMER_PRESCALER);
                switch(self.cycle_state){
                    case LED_BOOSTER_EVENT_START:
                        if(self.user.on_cycle(&self.rgb[0], &self.rgb[1], &self.rgb[2])){
                        }else{
                            self.state = LED_BOOSTER_STATE_COLD;
                        }
                        self.cycle_state = LED_BOOSTER_EVENT_RED;
                        //fallthrough
                    case LED_BOOSTER_EVENT_RED:
                        led_set(LED_RED_CHANNEL, self.rgb[0]);
                        self.cycle_state = LED_BOOSTER_EVENT_GREEN;
                        break;
                    case LED_BOOSTER_EVENT_GREEN:
                        led_set(LED_GREEN_CHANNEL, self.rgb[1]);
                        self.cycle_state = LED_BOOSTER_EVENT_BLUE;
                        break;
                    case LED_BOOSTER_EVENT_BLUE:
                        led_set(LED_BLUE_CHANNEL, self.rgb[2]);
                        self.cycle_state = LED_BOOSTER_EVENT_START;
                        break;
                }
                app_timer_start(self.timer, ticks, NULL);
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
    uint32_t ticks = APP_TIMER_TICKS(10, APP_TIMER_PRESCALER);
    app_timer_stop(self.timer);
    self.user.setup();
    self.state = LED_BOOSTER_STATE_WARM;
    app_timer_start(self.timer, ticks, NULL);
}
void led_booster_power_off(void){
    self.state = LED_BOOSTER_STATE_COLD;
}
