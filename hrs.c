#include <app_error.h>
#include <device_params.h>
#include <hrs.h>
#include <nrf.h>
#include <nrf_gpio.h>
#include <pwm.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

static uint32_t buckets[4];

void
ADC_IRQHandler(void)
{
    static uint32_t count;

    // Clear the END event
    NRF_ADC->EVENTS_END = 0;
    
    // Read the ADC result, and update the PWM channels accordingly
    uint8_t tmp = NRF_ADC->RESULT;// & HRS_MASK;
    if (tmp < BUCKET_LVL)
        ++buckets[0];
    else if (tmp < BUCKET_LVL*2)
        ++buckets[1];
    else if (tmp < BUCKET_LVL*3)
        ++buckets[2];
    else
        ++buckets[3];

    //PRINT_HEX(&tmp, 1);
    //PRINTC(' ');

    if (++count > 128) {
        PRINT_HEX(&buckets[0], 4);
        PRINT_HEX(&buckets[1], 4);
        PRINT_HEX(&buckets[2], 4);
        PRINT_HEX(&buckets[3], 4);
        PRINTS("\r\n");
        memset(buckets, 0, NUM_BUCKETS*sizeof(uint32_t));
        count = 0;
    }   

    // Trigger a new ADC sampling
    NRF_ADC->TASKS_START = 1;
}

void
adc_test() {
    uint32_t err_code;

    // Configure the ADC
    // P0.1 is used for ADC input, apply a varying voltage between 0 and VDD to change the PWM values
    NRF_ADC->CONFIG = ADC_CONFIG_RES_8bit << ADC_CONFIG_RES_Pos | \
                    ADC_CONFIG_INPSEL_AnalogInputNoPrescaling << ADC_CONFIG_INPSEL_Pos | \
                    ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos | \
                    HRS_ADC << ADC_CONFIG_PSEL_Pos;
    NRF_ADC->ENABLE = 1;  
    NRF_ADC->INTENSET = ADC_INTENSET_END_Msk;
    
    nrf_gpio_cfg_output(GPIO_3v3_Enable);
    nrf_gpio_pin_set(GPIO_3v3_Enable);

    //configure LED
    //nrf_gpio_cfg_output(GPIO_HRS_PWM_G);
    //nrf_gpio_pin_set  (GPIO_HRS_PWM_G);
    uint32_t gpios[] = {
        GPIO_HRS_PWM_G
    };

    err_code = pwm_init(PWM_1_Channel, gpios, PWM_Mode_20kHz_100);
    APP_ERROR_CHECK(err_code);
    err_code = pwm_set_value(PWM_1_Channel, 66);
    APP_ERROR_CHECK(err_code);

    // Enable the ADC interrupt, and set the priority to 1
    NVIC_SetPriority(ADC_IRQn, 1);
    NVIC_EnableIRQ(ADC_IRQn);   

    // Start the ADC
    NRF_ADC->TASKS_START = 1;
    
    while (true)
    {
        __WFE();
    }
}
