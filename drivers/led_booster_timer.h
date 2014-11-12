#pragma once
/**
 * This is the driver that provides timing for LED on the pill
 * that uses voltage booster
 * to prevent browning out of the system
 **/

typedef enum{
    LED_BOOSTER_EVENT_START = 0,
    LED_BOOSTER_EVENT_BLUE,
    LED_BOOSTER_EVENT_GREEN,
    LED_BOOSTER_EVENT_RED
}led_booster_event_t;

typedef struct{
    void (* setup )(void);
    void (* teardown)(void);
    void (* on_warm)(void);
    int  (* on_cycle)(led_booster_event_t event);
}led_booster_context_t;

//MUST CALL init before everything else
void led_booster_init(const led_booster_context_t * ctx);
void led_booster_power_on(void);
void led_booster_power_off(void);
