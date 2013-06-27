#include "micro-ecc/ecc.h"
#include <nrf_gpio.h>
#include <nrf_delay.h>

//John's hand soldered LED on the PCA10000
#define GPIO_LED 5

void
_start()
{
	nrf_gpio_cfg_output(GPIO_LED);

	
	while (1){
		nrf_gpio_pin_set(GPIO_LED);
		nrf_delay_ms(500);
		nrf_gpio_pin_clear(GPIO_LED);
		nrf_delay_ms(500);
	}

}
