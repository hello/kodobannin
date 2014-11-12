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

//indicates whether we may schedule power on
unsigned char led_booster_is_free(void);

//call to power on the routine
void led_booster_power_on(void);
//optional if we want to quit half way
void led_booster_power_off(void);
