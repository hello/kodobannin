#pragma once
/**
 * This is the driver that provides timing for LED on the pill
 * that uses voltage booster
 * to prevent browning out of the system
 **/


typedef struct{
    void (* setup )(void);
    void (* teardown)(void);
    void (* on_warm)(void);
    int  (* on_cycle)(int * out_r, int * out_g, int * out_b);
}led_booster_context_t;

//MUST CALL init before everything else
void led_booster_init(const led_booster_context_t * ctx);
void led_booster_power_on(void);
void led_booster_power_off(void);
