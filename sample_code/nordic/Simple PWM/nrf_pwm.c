#include "nrf_pwm.h"
#include "nrf.h"
#include "nrf_gpiote.h"
#include "nrf_gpio.h"

uint32_t pwm_max_value, pwm_next_value[PWM_NUM_CHANNELS], pwm_io_ch[PWM_NUM_CHANNELS], pwm_running[PWM_NUM_CHANNELS], pwm_temp_inten;
const uint32_t pwm_gpiote_channel[3] = {PWM_GPIOTE_CHANNEL0, PWM_GPIOTE_CHANNEL1, PWM_GPIOTE_CHANNEL2};

int32_t nrf_ppi_enable_first_available_channel(volatile uint32_t *event_ptr, volatile uint32_t *task_ptr)
{
    uint32_t channel_num;
    // Traverse the total list of channels
    for(channel_num = 0; channel_num < 16; channel_num++)
    {
        // Look for a channel that is not set
        if((NRF_PPI->CHEN & (1 << channel_num)) == 0)
        {
            // When a free channel is found, exit the loop
            break;
        }
    }

    if(channel_num >= 16)
    {
        // If the loop reaches the end no channels are free, and we exit returning -1
        return -1;
    }
    else
    {
        // Otherwise we configure the channel and return the channel number
        *(&(NRF_PPI->CH0_EEP) + (channel_num * 2)) = (uint32_t)event_ptr;
        *(&(NRF_PPI->CH0_TEP) + (channel_num * 2)) = (uint32_t)task_ptr;    
        NRF_PPI->CHENSET = (1 << channel_num);   
        return channel_num;
    }
}

void pwm_init_common(nrf_pwm_mode_t pwm_mode)
{
	PWM_TIMER->TASKS_CLEAR = 1;
	PWM_TIMER->BITMODE = TIMER_BITMODE_BITMODE_16Bit;
    switch(pwm_mode)
    {
        case PWM_MODE_LED_100:   // 0-100 resolution, 156 Hz PWM frequency, 32 kHz timer frequency (prescaler 9)
            PWM_TIMER->PRESCALER = 9; /* Prescaler 4 results in 1 tick == 1 microsecond */
            pwm_max_value = 100;
            break;
        case PWM_MODE_LED_255:   // 8-bit resolution, 122 Hz PWM frequency, 65 kHz timer frequency (prescaler 8)
            PWM_TIMER->PRESCALER = 8;
            pwm_max_value = 255;        
            break;
        case PWM_MODE_LED_1000:  // 0-1000 resolution, 250 Hz PWM frequency, 500 kHz timer frequency (prescaler 5)
            PWM_TIMER->PRESCALER = 5;
            pwm_max_value = 1000;
            break;
        case PWM_MODE_MTR_100:   // 0-100 resolution, 20 kHz PWM frequency, 4MHz timer frequency (prescaler 2)
            PWM_TIMER->PRESCALER = 2;
            pwm_max_value = 100;
            break;
        case PWM_MODE_MTR_255:    // 8-bit resolution, 31 kHz PWM frequency, 16MHz timer frequency (prescaler 0)	
            PWM_TIMER->PRESCALER = 0;
            pwm_max_value = 255;
            break;
    }
    PWM_TIMER->CC[3] = pwm_max_value*2;
	PWM_TIMER->MODE = TIMER_MODE_MODE_Timer;
    PWM_TIMER->SHORTS = TIMER_SHORTS_COMPARE3_CLEAR_Msk;
    PWM_TIMER->EVENTS_COMPARE[0] = PWM_TIMER->EVENTS_COMPARE[1] = PWM_TIMER->EVENTS_COMPARE[2] = PWM_TIMER->EVENTS_COMPARE[3] = 0;     

    for(int i = 0; i < PWM_NUM_CHANNELS; i++)
    {
        nrf_ppi_enable_first_available_channel(&PWM_TIMER->EVENTS_COMPARE[i], &NRF_GPIOTE->TASKS_OUT[pwm_gpiote_channel[i]]);
        nrf_ppi_enable_first_available_channel(&PWM_TIMER->EVENTS_COMPARE[3], &NRF_GPIOTE->TASKS_OUT[pwm_gpiote_channel[i]]);        
    }
    NVIC_SetPriority(PWM_IRQn, 3);
    NVIC_EnableIRQ(PWM_IRQn);
    PWM_TIMER->TASKS_START = 1;
}
#if (PWM_NUM_CHANNELS == 1)
void nrf_pwm_init(uint32_t io_select_pwm0, nrf_pwm_mode_t pwm_mode)
{
    pwm_io_ch[0] = io_select_pwm0;
    nrf_gpio_cfg_output(io_select_pwm0);
    pwm_running[0] = 0;    

    pwm_init_common(pwm_mode);
}
#elif (PWM_NUM_CHANNELS == 2)
void nrf_pwm_init(uint32_t io_select_pwm0, uint32_t io_select_pwm1, nrf_pwm_mode_t pwm_mode)
{
    pwm_io_ch[0] = io_select_pwm0;
    nrf_gpio_cfg_output(io_select_pwm0);
    pwm_io_ch[1] = io_select_pwm1;
    nrf_gpio_cfg_output(io_select_pwm1);
    pwm_running[0] = pwm_running[1] = 0;
    
    pwm_init_common(pwm_mode);
}
#elif (PWM_NUM_CHANNELS == 3)
void nrf_pwm_init(uint32_t io_select_pwm0, uint32_t io_select_pwm1, uint32_t io_select_pwm2, nrf_pwm_mode_t pwm_mode)
{
    pwm_io_ch[0] = io_select_pwm0;
    nrf_gpio_cfg_output(io_select_pwm0);
    pwm_io_ch[1] = io_select_pwm1;
    nrf_gpio_cfg_output(io_select_pwm1);
    pwm_io_ch[2] = io_select_pwm2;
    nrf_gpio_cfg_output(io_select_pwm2);
    pwm_running[0] = pwm_running[1] = pwm_running[2] = 0;    
    pwm_init_common(pwm_mode);
}
#endif
void nrf_pwm_set_value(uint32_t pwm_channel, uint32_t pwm_value)
{
    pwm_next_value[pwm_channel] = pwm_value;
    PWM_TIMER->EVENTS_COMPARE[3] = 0;
    PWM_TIMER->SHORTS = TIMER_SHORTS_COMPARE3_CLEAR_Msk | TIMER_SHORTS_COMPARE3_STOP_Msk;
    PWM_TIMER->INTENSET = TIMER_INTENSET_COMPARE3_Msk;    
    PWM_TIMER->TASKS_START = 1;
}
void PWM_IRQHandler(void)
{
    static uint32_t i;
    PWM_TIMER->EVENTS_COMPARE[3] = 0;
    PWM_TIMER->INTENCLR = 0xFFFFFFFF;
    for(i = 0; i < PWM_NUM_CHANNELS; i++)
    {
        if(pwm_next_value[i] == 0)
        {
            nrf_gpiote_unconfig(pwm_gpiote_channel[i]);
            nrf_gpio_pin_write(pwm_io_ch[i], 0);
            pwm_running[i] = 0;
        }
        else if (pwm_next_value[i] >= pwm_max_value)
        {
            nrf_gpiote_unconfig(pwm_gpiote_channel[i]);
            nrf_gpio_pin_write(pwm_io_ch[i], 1); 
            pwm_running[i] = 0;
        }
        else
        {
            PWM_TIMER->CC[i] = pwm_next_value[i] * 2;
            if(!pwm_running[i])
            {
                nrf_gpiote_task_config(pwm_gpiote_channel[i], pwm_io_ch[i], NRF_GPIOTE_POLARITY_TOGGLE, NRF_GPIOTE_INITIAL_VALUE_HIGH);  
                pwm_running[i] = 1;
            }
        }
    }
    PWM_TIMER->SHORTS = TIMER_SHORTS_COMPARE3_CLEAR_Msk;
    PWM_TIMER->TASKS_START = 1;
}
