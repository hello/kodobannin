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
    gpio_cfg_d0s1_opendrain_output_connect(VLED_VDD_EN, 0, NRF_GPIO_PIN_PULLDOWN);  // on: 0  VDD on
    gpio_cfg_s0s1_output_connect(VRGB_ENABLE, 1);  // on: 1  Boost on

    // 1 + NRF_GPIO_PIN_PULLUP  = RED ONLY + NO PWM ADJUST <- VERY TINY RED
    // 1 + NRF_GPIO_PIN_NOPULL  = RED ONLY + PWM ADJUST
    // 1 + NRF_GPIO_PIN_PULLDOWN = CONNECTED_COLOR + PWM AJUST
    // 0 + NRF_GPIO_PIN_NOPULL  = CONNECTED_COLOR + PWM AJUST
    // 0 + NRF_GPIO_PIN_PULLUP  = CONNECTED_COLOR + PWM ADJUST
    // 0 + NRF_GPIO_PIN_PULLDOWN = CONNECTED_COLOR + PWM ADJUST

    // Note: Make sure to satrt with 0 + NRF_GPIO_PIN_NOPULL, or you will get a crash
    // when you change the SELECT state.
    gpio_cfg_h0d1_opendrain_output_connect(VRGB_SELECT, 0, NRF_GPIO_PIN_NOPULL);  // on: 0  Ground SELECT
    
    uint32_t gpios[1] = {VRGB_ADJUST};
    
    APP_OK(pwm_init(PWM_1_Channel, gpios, PWM_Mode_122Hz_255));
    APP_OK(pwm_set_value(PWM_Channel_1, 0xAF));

    gpio_cfg_s0s1_output_connect(LED3_ENABLE, 1);  // Green
    gpio_cfg_s0s1_output_connect(LED2_ENABLE, 1);  // Red
    gpio_cfg_s0s1_output_connect(LED1_ENABLE, 1);  //Blue
    
#endif
}

void led_all_colors_off()
{
#ifdef PLATFORM_HAS_VLED

    nrf_gpio_pin_set(LED3_ENABLE);
    nrf_gpio_pin_set(LED2_ENABLE);
    nrf_gpio_pin_set(LED1_ENABLE);
    
#endif
}

void led_all_colors_on()
{
#ifdef PLATFORM_HAS_VLED
    
    nrf_gpio_pin_clear(LED3_ENABLE);
    nrf_gpio_pin_clear(LED2_ENABLE);
    nrf_gpio_pin_clear(LED1_ENABLE);
    
#endif
}


void led_power_off()
{
#ifdef PLATFORM_HAS_VLED
    pwm_disable();

    {
        // LEDS disconnect
        gpio_cfg_d0s1_output_disconnect(LED3_ENABLE);  // Green
        gpio_cfg_d0s1_output_disconnect(LED2_ENABLE);  // Red
        gpio_cfg_d0s1_output_disconnect(LED1_ENABLE);  //Blue
    }

    {
        // tri state output RGB select
        gpio_cfg_h0d1_opendrain_output_connect(VRGB_SELECT, 0, NRF_GPIO_PIN_NOPULL);  // on: 0  Ground SELECT
        gpio_cfg_d0s1_output_disconnect(VRGB_SELECT);
    }
    
    {
        gpio_cfg_d0s1_output_disconnect(VRGB_ADJUST);  //PWM
    }

    {
        // Boost off and disconnect

        gpio_cfg_s0s1_output_connect(VRGB_ENABLE, 0);
        gpio_cfg_d0s1_output_disconnect(VRGB_ENABLE);  // on: 1  Boost 5mA leak
    }
    nrf_delay_ms(100);
    
    {
        // VDD off and disconnect
        //gpio_cfg_d0s1_opendrain_output_connect(VLED_VDD_EN, 1, NRF_GPIO_PIN_PULLDOWN);
        gpio_cfg_d0s1_output_disconnect_pull(VLED_VDD_EN, NRF_GPIO_PIN_PULLDOWN);
    }

#endif    
}

static void _led_blink_all(void* ctx)
{
    if(_blink_count % 2 == 0)
    {
        led_all_colors_on();
    }else{
        led_all_colors_off();

        if(_blink_count == 1)
        {
            nrf_delay_ms(10);
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
    nrf_delay_ms(10);
    APP_OK(app_timer_start(_flash_timer, LED_INIT_LIGHTUP_INTERAVL, NULL));

#endif
}
