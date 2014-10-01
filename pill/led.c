#include <stdlib.h>

#include "nrf_gpio.h"

#include "app.h"
#include "platform.h"
#include "util.h"
#include "nrf_gpio.h"

#include "led.h"

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

void led_power_on()
{
    nrf_gpio_cfg_output(VLED_VDD_EN);
    nrf_gpio_pin_clear(VLED_VDD_EN); // write 0

    nrf_gpio_cfg_output(VRGB_ENABLE);
    nrf_gpio_pin_set(VRGB_ENABLE);  // write 1

    nrf_gpio_cfg_output(VRGB_SELECT);
    nrf_gpio_pin_clear(VRGB_SELECT);  // write 1
    

    /*
    nrf_gpio_cfg_output(VRGB_ADJUST);
    nrf_gpio_pin_clear(VRGB_ADJUST);  // write 1
    */


    nrf_gpio_cfg_output(LED3_ENABLE);
    nrf_gpio_pin_clear(LED3_ENABLE);  // write 0
    

    /*
    nrf_gpio_cfg_output(LED2_ENABLE);
    nrf_gpio_pin_clear(LED2_ENABLE);  // write 0
    */
    
    nrf_gpio_cfg_output(LED1_ENABLE);
    nrf_gpio_pin_clear(LED1_ENABLE);  // write 0

    PRINTS("LED powered on.\r\n");
    
}


void led_power_off()
{
    nrf_gpio_cfg_output(VLED_VDD_EN);
    nrf_gpio_pin_set(VLED_VDD_EN);

    nrf_gpio_cfg_output(VRGB_ENABLE);
    nrf_gpio_pin_clear(VRGB_ENABLE);
}



#endif