#include <stdlib.h>

#include "nrf_gpio.h"

#include "app.h"
#include "platform.h"
#include "util.h"
#include "nrf_gpio.h"
#include "gpio_nor.h"

#include "led.h"

#ifdef PLATFORM_HAS_VLED


void led_power_on()
{
    
    nrf_gpio_cfg_output(VLED_VDD_EN);
    nrf_gpio_pin_clear(VLED_VDD_EN); // on: 0
    
    

    
    nrf_gpio_cfg_output(VRGB_ENABLE);
    nrf_gpio_pin_set(VRGB_ENABLE);  // on: 1
    
    
    nrf_gpio_cfg_output(VRGB_SELECT);
    nrf_gpio_pin_clear(VRGB_SELECT);  // on: 1

    
    //nrf_gpio_cfg_output(VRGB_ADJUST);
    //nrf_gpio_pin_clear(VRGB_ADJUST);  // on: 1
    
    
    nrf_gpio_cfg_output(LED3_ENABLE);
    nrf_gpio_pin_set(LED3_ENABLE);  // on: 0
    
    
    nrf_gpio_cfg_output(LED2_ENABLE);
    nrf_gpio_pin_set(LED2_ENABLE);  // on: 0
    
    
    nrf_gpio_cfg_output(LED1_ENABLE);
    nrf_gpio_pin_set(LED1_ENABLE);  // on: 0
    
    

    

    
    /*

    gpio_cfg_s0s1_output_connect(VLED_VDD_EN, 0);
    gpio_cfg_s0s1_output_connect(VRGB_ENABLE, 1);
    gpio_cfg_s0s1_output_connect(VRGB_SELECT, 0);

    gpio_cfg_s0s1_output_connect(LED3_ENABLE, 1);  // Green
    gpio_cfg_s0s1_output_connect(LED2_ENABLE, 1);  // Red
    gpio_cfg_s0s1_output_connect(LED1_ENABLE, 1);  //Blue
    */

    //PRINTS("LED powered on.\r\n");
    
}


void led_all_colors_on()
{
    nrf_gpio_pin_clear(LED3_ENABLE);
    nrf_gpio_pin_clear(LED2_ENABLE);
    nrf_gpio_pin_clear(LED1_ENABLE);
}


void led_power_off()
{
    /*
    nrf_gpio_cfg_output(LED3_ENABLE);
    nrf_gpio_pin_set(LED3_ENABLE);  // write 0
    
    nrf_gpio_cfg_output(LED2_ENABLE);
    nrf_gpio_pin_set(LED2_ENABLE);  // write 0
    
    
    nrf_gpio_cfg_output(LED1_ENABLE);
    nrf_gpio_pin_set(LED1_ENABLE);  // write 0
    */

    gpio_cfg_s0s1_output_connect(LED3_ENABLE, 1);
    gpio_cfg_d0s1_output_disconnect(LED3_ENABLE);
    gpio_cfg_s0s1_output_connect(LED2_ENABLE, 1);
    gpio_cfg_d0s1_output_disconnect(LED2_ENABLE);
    gpio_cfg_s0s1_output_connect(LED1_ENABLE, 1);
    gpio_cfg_d0s1_output_disconnect(LED1_ENABLE);

    /*
    nrf_gpio_cfg_output(VRGB_SELECT);
    nrf_gpio_pin_clear(VRGB_SELECT);  // on: 1
    */
    gpio_cfg_s0s1_output_connect(VRGB_SELECT, 0);
    gpio_cfg_d0s1_output_disconnect(VRGB_SELECT);


    /*
    nrf_gpio_cfg_output(VRGB_ENABLE);
    nrf_gpio_pin_clear(VRGB_ENABLE);
    */
    gpio_cfg_s0s1_output_connect(VRGB_ENABLE, 0);
    gpio_cfg_d0s1_output_disconnect(VRGB_ENABLE);


    /*
    nrf_gpio_cfg_output(VLED_VDD_EN);
    nrf_gpio_pin_set(VLED_VDD_EN);
    */
    gpio_cfg_s0s1_output_connect(VLED_VDD_EN, 1);
    gpio_cfg_d0s1_output_disconnect(VLED_VDD_EN);

    
}



#endif