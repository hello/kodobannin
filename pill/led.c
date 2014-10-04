#include <stdlib.h>

#include <nrf_gpio.h>
#include <app_timer.h>

#include "app.h"
#include "platform.h"
#include "util.h"
#include "gpio_nor.h"
#include "pwm.h"

#include "led.h"


static led_callback_t _on_falsh_finished;
static app_timer_id_t _flash_timer;
static volatile uint8_t _blink_count;

void led_power_on()
{
#ifdef PLATFORM_HAS_VLED
    gpio_cfg_s0s1_output_connect(VLED_VDD_EN, 0);  // on: 0
    gpio_cfg_s0s1_output_connect(VRGB_ENABLE, 1);  // on: 1  Boost
    gpio_cfg_s0s1_output_connect(VRGB_SELECT, 0);  // on: 0
    
    /*
    nrf_pwm_init(VRGB_ADJUST, PWM_MODE_LED_255);
    nrf_pwm_set_value(0, 0xA0);
    */
    
    gpio_cfg_s0s1_output_connect(LED3_ENABLE, 1);  // Green
    gpio_cfg_s0s1_output_connect(LED2_ENABLE, 1);  // Red
    gpio_cfg_s0s1_output_connect(LED1_ENABLE, 1);  //Blue
#endif   
}

void led_all_colors_off()
{
#ifdef PLATFORM_HAS_VLED
    /*if(ret != 0){
        nrf_gpio_pin_set(LED3_ENABLE);
        nrf_gpio_pin_clear(LED2_ENABLE);
        nrf_gpio_pin_set(LED1_ENABLE);
    }else{
        nrf_gpio_pin_clear(LED3_ENABLE);
        nrf_gpio_pin_clear(LED2_ENABLE);
        nrf_gpio_pin_clear(LED1_ENABLE);
    }*/
    nrf_gpio_pin_set(LED3_ENABLE);
    nrf_gpio_pin_set(LED2_ENABLE);
    nrf_gpio_pin_set(LED1_ENABLE);
#endif
}

void led_all_colors_on()
{
#ifdef PLATFORM_HAS_VLED
    /*if(ret != 0){
        nrf_gpio_pin_set(LED3_ENABLE);
        nrf_gpio_pin_clear(LED2_ENABLE);
        nrf_gpio_pin_set(LED1_ENABLE);
    }else{
        nrf_gpio_pin_clear(LED3_ENABLE);
        nrf_gpio_pin_clear(LED2_ENABLE);
        nrf_gpio_pin_clear(LED1_ENABLE);
    }*/

    uint32_t gpios[1] = {VRGB_ADJUST};
    //APP_OK(pwm_init(PWM_1_Channel, gpios, PWM_Mode_122Hz_255));
    //APP_OK(pwm_set_value(PWM_Channel_1, 0xA0));

    nrf_gpio_pin_clear(LED3_ENABLE);
    nrf_gpio_pin_clear(LED2_ENABLE);
    nrf_gpio_pin_clear(LED1_ENABLE);
#endif
}


void led_power_off()
{
#ifdef PLATFORM_HAS_VLED
    {
        // LEDS off
    gpio_cfg_d0s1_output_disconnect(LED3_ENABLE);  // Green
    gpio_cfg_d0s1_output_disconnect(LED2_ENABLE);  // Red
    gpio_cfg_d0s1_output_disconnect(LED1_ENABLE);  //Blue
    }
    
    gpio_cfg_s0s1_output_connect(VRGB_ENABLE, 0);
    gpio_cfg_d0s1_output_disconnect(VRGB_ENABLE);  // on: 1  Boost 5mA leak

    gpio_cfg_s0s1_output_connect(VLED_VDD_EN, 1);
    gpio_cfg_d0s1_output_disconnect(VLED_VDD_EN);


#endif    
}

static void _led_blink_all(void* ctx)
{
    if(_blink_count % 2 == 1)
    {
        led_all_colors_on();
    }else{
        led_all_colors_off();

        if(_blink_count == 0)
        {
            led_power_off();
            if(_on_falsh_finished)
            {
                _on_falsh_finished();
            }
            _blink_count = 0;
            return;
        }
    }

    _blink_count--;
    APP_OK(app_timer_start(_flash_timer, LED_INIT_LIGHTUP_INTERAVL, NULL));
}

bool led_flash(uint32_t color_rgb, uint8_t count, led_callback_t on_finished)
{
    if(_blink_count > 0)
    {
        return false;
    }

#ifdef PLATFORM_HAS_VLED
    if(!on_finished)
    {
        _on_falsh_finished = on_finished;
    }

    if(_flash_timer)
    {
        app_timer_stop(_flash_timer);
    }else{
        APP_OK(app_timer_create(&_flash_timer, APP_TIMER_MODE_SINGLE_SHOT, _led_blink_all));
    }
    _blink_count = count * 2;
    led_power_on();
    APP_OK(app_timer_start(_flash_timer, LED_INIT_LIGHTUP_INTERAVL, NULL));

#endif
}
