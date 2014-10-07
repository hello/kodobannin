#include <stdlib.h>

#include <app_timer.h>

#include "platform.h"
#include "app.h"
#include "util.h"
#include "nrf_gpio.h"

#include "led.h"

static led_callback_t _on_falsh_finished;
static app_timer_id_t _flash_timer;
static volatile uint8_t _blink_count;


#ifdef PLATFORM_HAS_VLED

static __INLINE void _led_gpio_cfg_tri_state_output(uint32_t pin_number)
{
    /*lint -e{845} // A zero has been given as right argument to operator '|'" */
    NRF_GPIO->PIN_CNF[pin_number] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
                                            | (GPIO_PIN_CNF_DRIVE_D0H1 << GPIO_PIN_CNF_DRIVE_Pos)
                                            | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
                                            | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos)
                                            | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
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
    APP_OK(app_timer_start(_flash_timer, LED_INIT_LIGHTUP_INTERAVL, NULL));
#endif
}

void led_all_colors_on()
{
#ifdef PLATFORM_HAS_VLED
    nrf_gpio_cfg_output(LED3_ENABLE);
    nrf_gpio_pin_clear(LED3_ENABLE);

    nrf_gpio_cfg_output(LED2_ENABLE);
    nrf_gpio_pin_clear(LED2_ENABLE);

    nrf_gpio_cfg_output(LED1_ENABLE);
    nrf_gpio_pin_clear(LED1_ENABLE);
#endif
}

void led_all_colors_off()
{
#ifdef PLATFORM_HAS_VLED
    nrf_gpio_cfg_output(LED3_ENABLE);
    nrf_gpio_pin_set(LED3_ENABLE);

    nrf_gpio_cfg_output(LED2_ENABLE);
    nrf_gpio_pin_set(LED2_ENABLE);

    nrf_gpio_cfg_output(LED1_ENABLE);
    nrf_gpio_pin_set(LED1_ENABLE);
#endif
}

void led_power_on()
{
#ifdef PLATFORM_HAS_VLED
    PRINTS("LED adjust for soft start\r\n");
    nrf_gpio_cfg_output(VRGB_ADJUST);
    nrf_gpio_pin_set(VRGB_ADJUST);  // write 1

    nrf_delay_ms(10);

    PRINTS("LED boost enabled\r\n");
    nrf_gpio_cfg_output(VRGB_ENABLE);
    nrf_gpio_pin_set(VRGB_ENABLE);  // write 1

    nrf_delay_ms(10);

    PRINTS("LED power set high\r\n");
    nrf_gpio_cfg_output(VRGB_SELECT); // grn / blu
    nrf_gpio_pin_clear(VRGB_SELECT);  // write 0

    nrf_delay_ms(10);

    PRINTS("LED power enable\r\n");
    nrf_gpio_cfg_output(VLED_VDD_EN);
    nrf_gpio_pin_clear(VLED_VDD_EN); // write 0
   
    nrf_delay_ms(10);

    PRINTS("Soft start finished\r\n");

    
 // nrf_gpio_cfg_output(LED2_ENABLE); // red grn
 // nrf_gpio_pin_clear(LED2_ENABLE);  // write 0
    
    
 // nrf_gpio_cfg_output(LED1_ENABLE); // blu blu
 // nrf_gpio_pin_clear(LED1_ENABLE);  // write 0

 /* nrf_delay_ms(400);

    PRINTS("LED perform soft start\r\n");
    nrf_gpio_cfg_output(VRGB_ADJUST);
    nrf_gpio_pin_clear(VRGB_ADJUST);  // write 0
 */
 /* nrf_delay_ms(400);

    PRINTS("LED disabled\r\n");
    nrf_gpio_cfg_output(LED2_ENABLE); // red grn
    nrf_gpio_pin_set(LED2_ENABLE);  // write 1
   
    nrf_delay_ms(400);

    PRINTS("LED adjust for soft start\r\n");
    nrf_gpio_cfg_output(VRGB_ADJUST);
    nrf_gpio_pin_set(VRGB_ADJUST);  // write 1

    nrf_delay_ms(400);

    PRINTS("LED power set high\r\n");
    nrf_gpio_cfg_output(VRGB_SELECT); // grn / blu
    nrf_gpio_pin_clear(VRGB_SELECT);  // write 0
   
    nrf_delay_ms(400);

    PRINTS("LED perform for soft start\r\n");
    nrf_gpio_cfg_output(VRGB_ADJUST);
    nrf_gpio_pin_clear(VRGB_ADJUST);  // write 0

    PRINTS("LED enabled\r\n");
    nrf_gpio_cfg_output(LED2_ENABLE); // red grn
    nrf_gpio_pin_clear(LED2_ENABLE);  // write 0
 */
 /* nrf_delay_ms(400);

    PRINTS("LED powered on higher.\r\n");
    nrf_gpio_cfg_output(VRGB_ADJUST);
    nrf_gpio_pin_clear(VRGB_ADJUST);  // write 0
 */
 /* nrf_delay_ms(400);

    PRINTS("LED Grn/Blu select\r\n");
    nrf_gpio_cfg_output(VRGB_ADJUST);
    nrf_gpio_pin_clear(VRGB_ADJUST);  // write 0
 */
 /* nrf_delay_ms(400);

    PRINTS("LED powered on high.\r\n");
    nrf_gpio_cfg_output(VRGB_SELECT); // grn / blu
    nrf_gpio_pin_clear(VRGB_SELECT);  // write 1
 */
 /* nrf_delay_ms(400);

    PRINTS("LED powered lower.\r\n");
    nrf_gpio_cfg_output(VRGB_ADJUST);
    nrf_gpio_pin_set(VRGB_ADJUST);  // write 1
 */
 /* nrf_delay_ms(400);

    PRINTS("LED powered on low.\r\n");
    nrf_gpio_cfg_output(VRGB_SELECT); // grn / blu
    nrf_gpio_pin_set(VRGB_SELECT);  // write 1
 */
 /* nrf_delay_ms(400);

    PRINTS("LED powered off.\r\n");
    nrf_gpio_cfg_output(VRGB_ENABLE);
    nrf_gpio_pin_set(VRGB_ENABLE);  // write 1

    nrf_delay_ms(400);
 */
 /* PRINTS("LED powered off.\r\n");
    nrf_gpio_cfg_output(VLED_VDD_EN);
    nrf_gpio_pin_set(VLED_VDD_EN); // write 1
 */

    PRINTS("LED power exit w/delay.\r\n");
#endif
}


void led_power_off()
{
#ifdef PLATFORM_HAS_VLED
    nrf_gpio_cfg_output(VLED_VDD_EN);
    nrf_gpio_pin_set(VLED_VDD_EN);
    //gpio_cfg_d0s1_output_disconnect(VLED_VDD_EN);

    nrf_gpio_cfg_output(VRGB_ENABLE);
    nrf_gpio_pin_clear(VRGB_ENABLE);
    //gpio_cfg_d0s1_output_disconnect(VRGB_ENABLE);
#endif
}



#endif