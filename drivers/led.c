#include <stdlib.h>

#include <app_timer.h>

#include "platform.h"
#include "app.h"
#include "util.h"
#include "nrf_gpio.h"

#include "led.h"
#include "pwm.h"

static led_callback_t _on_falsh_finished;
static app_timer_id_t _flash_timer;
static volatile uint16_t _blink_color;
static volatile uint16_t _blink_count;


#ifdef PLATFORM_HAS_VLED

static __INLINE void _led_gpio_cfg_open_drain(uint32_t pin_number)
{
    /*lint -e{845} // A zero has been given as right argument to operator '|'" */
    NRF_GPIO->PIN_CNF[pin_number] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) // no edge detect
                                  | (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos)     // nfet pull down only
                                  | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)   // no resistor pull
                                  | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos)  // no input
                                  | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);      // output only
}

static void _led_blink_all(void* ctx)
{
    uint8_t count;

    if(_blink_count % 2 == 0)
    {
     // APP_OK(pwm_set_value(PWM_Channel_1, 0x0F));
     // led_all_colors_on();
        if ((_blink_color % 3) == 0) nrf_gpio_pin_clear(LED3_ENABLE); // red
        if ((_blink_color % 3) == 1) nrf_gpio_pin_clear(LED2_ENABLE); // grn
        if ((_blink_color % 3) == 2) nrf_gpio_pin_clear(LED1_ENABLE); // blu
    }else{
        if(_blink_count == 1)
        {

            led_power_off();
            if(_on_falsh_finished)
            {
                _on_falsh_finished();
            }
            _blink_count = 0;
            return;
        } else {
            led_all_colors_off();
            _blink_color++; if (_blink_color > 2) _blink_color= 0;
            if (_blink_color % 3) { // grn/blu selected
             // nrf_gpio_pin_clear(VRGB_SELECT);  // grn/blu ( high volt range )
                if ((_blink_color % 3) == 1) { // grn selected
                    APP_OK(pwm_set_value(PWM_Channel_1, 0x37)); // adjust pwm trim lower
                } else { // blu selected
                    APP_OK(pwm_set_value(PWM_Channel_1, 0x2F)); // adjust pwm trim higher
                }
            } else { // red led selected
             // nrf_gpio_pin_set(VRGB_SELECT);  // red ( low volt range )
                APP_OK(pwm_set_value(PWM_Channel_1, 0x3F)); // adjust pwm trim null
            }

         // count = (uint8_t) _blink_count / 6;
         // PRINT_HEX(&count, sizeof(count));
        }
    }
    _blink_count--;
    APP_OK(app_timer_start(_flash_timer, LED_INIT_LIGHTUP_INTERAVL, NULL));
}

bool led_flash(uint8_t color, uint8_t count, led_callback_t on_finished) // uint32_t color_rgb
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

    _blink_color = color;
    _blink_count = count * 2 * 3; // turn all three colors on/off each cycle

    led_power_on();

    APP_OK(app_timer_start(_flash_timer, LED_INIT_LIGHTUP_INTERAVL, NULL));
#endif
}

void led_init()
{
    nrf_gpio_pin_set(VLED_VDD_EN); // set pfet gate high
    nrf_gpio_cfg_output(VLED_VDD_EN); // power off boost regualator

    nrf_gpio_pin_clear(VRGB_ENABLE); // set control input low
    nrf_gpio_cfg_output(VRGB_ENABLE); // disable boost regulator enable input

    nrf_gpio_pin_clear(VRGB_SELECT);  // connect from boost resistor divider ( FB set )
    _led_gpio_cfg_open_drain(VRGB_SELECT); // voltage range select ( grn/blu vs red )

    nrf_gpio_pin_set(VRGB_ADJUST); // disconnect from boost adjust filter ( FB trim )
    _led_gpio_cfg_open_drain(VRGB_ADJUST); // dac output w/pwm filter ( 100K / 0.1uF )

    nrf_gpio_pin_set(LED3_ENABLE); // red led off ( open drain )
    _led_gpio_cfg_open_drain(LED3_ENABLE); // nrf_gpio_cfg_output(LED3_ENABLE); // red

    nrf_gpio_pin_set(LED2_ENABLE); // grn led off ( open drain )
    _led_gpio_cfg_open_drain(LED2_ENABLE); // nrf_gpio_cfg_output(LED2_ENABLE); // grn

    nrf_gpio_pin_set(LED1_ENABLE); // blu led off ( open drain )
    _led_gpio_cfg_open_drain(LED1_ENABLE); // nrf_gpio_cfg_output(LED1_ENABLE); // blu

}


void led_all_colors_on()
{
#ifdef PLATFORM_HAS_VLED
    nrf_gpio_pin_clear(LED3_ENABLE); // red
    nrf_gpio_pin_clear(LED2_ENABLE); // grn
    nrf_gpio_pin_clear(LED1_ENABLE); // blu
#endif
}

void led_all_colors_off()
{
#ifdef PLATFORM_HAS_VLED
    nrf_gpio_pin_set(LED3_ENABLE); // red
    nrf_gpio_pin_set(LED2_ENABLE); // grn
    nrf_gpio_pin_set(LED1_ENABLE); // blu
#endif
}

void led_warm_up(){
    PRINTS(" boost");
    nrf_gpio_pin_clear(VLED_VDD_EN); // write 0 to enable pfet power control
    PRINTS(" on"); // boost regulator powered on
    PRINTS(" set )===\r\n"); // boost regulator powered on
}
void led_power_on()
{
#ifdef PLATFORM_HAS_VLED
    uint32_t gpios[1] = {VRGB_ADJUST}; // port to use for pwm dac

    PRINTS("\r\n===( LED precharge");
    nrf_gpio_pin_set(VRGB_ENABLE);  // precharge capacitors ( Vrgb / Vpwm )

    PRINTS(" pwm");
    APP_OK(pwm_init(PWM_1_Channel, gpios, PWM_Mode_32kHz_255));
    APP_OK(pwm_set_value(PWM_Channel_1, 0xEF)); // set initial Vrgb = Vmcu

    //nrf_delay_ms(5); // wait for Vrgb to settle ( diode drop below Vdd )

 // nrf_gpio_cfg_output(LED3_ENABLE); // grn red
 // nrf_gpio_pin_clear(LED3_ENABLE);  // write 0
    
 // nrf_gpio_cfg_output(LED2_ENABLE); // orn grn
 // nrf_gpio_pin_clear(LED2_ENABLE);  // write 0
    
 // nrf_gpio_cfg_output(LED1_ENABLE); // blu blu
 // nrf_gpio_pin_clear(LED1_ENABLE);  // write 0

#endif
}


void led_power_off()
{
#ifdef PLATFORM_HAS_VLED
    pwm_disable();

    nrf_gpio_pin_clear(VRGB_ENABLE); // boost disabled and then
    nrf_gpio_pin_set(VLED_VDD_EN); // regulator powered off
#endif
}



#endif
