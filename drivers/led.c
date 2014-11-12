#include <stdlib.h>

#include <app_timer.h>

#include "app.h"
#include "util.h"
#include "nrf_gpio.h"

#include "led.h"
#include "pwm.h"


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

void led_set(int led_channel, int pwmval){
    int offset = 0;
    led_all_colors_off();
    if(led_channel == LED_GREEN_CHANNEL){
        offset = 0x8;
    }
    if(pwmval < 0xff && pwmval - offset > 0){
        nrf_gpio_pin_clear(led_channel);
        APP_OK(pwm_set_value(PWM_Channel_1, pwmval - offset));
    }

}

void led_init()
{
    uint32_t gpios[1] = {VRGB_ADJUST}; // port to use for pwm dac

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

    APP_OK(pwm_init(PWM_1_Channel, gpios, PWM_Mode_32kHz_255));
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

    PRINTS("\r\n===( LED precharge");
    nrf_gpio_pin_set(VRGB_ENABLE);  // precharge capacitors ( Vrgb / Vpwm )

    PRINTS(" pwm");
    APP_OK(pwm_set_value(PWM_Channel_1, 0xEF)); // set initial Vrgb = Vmcu

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
