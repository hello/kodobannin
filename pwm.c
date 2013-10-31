#include <app_error.h>
#include <device_params.h>
#include <pwm.h>
#include <nrf.h>
#include <nrf_delay.h>
#include <nrf_gpiote.h>
#include <nrf_gpio.h>
#include <nrf_soc.h>
#include <util.h>

#include "device_params.h"

static PWM_Mode    _mode  = PWM_Mode_Invalid;
static PWM_Channel _chans = PWM_Num_Channels;

uint32_t pwm_max_value;
uint32_t pwm_next_value[PWM_Num_Channels];
uint32_t pwm_io_ch[PWM_Num_Channels];
uint32_t pwm_running[PWM_Num_Channels];
uint32_t pwm_temp_inten;

const uint32_t pwm_gpiote_channel[] = {
	PWM_GPIOTE_CHANNEL0,
	PWM_GPIOTE_CHANNEL1,
	PWM_GPIOTE_CHANNEL2
};

static const PWM_Config _configs[] = {
	{  100, 9 }, // 0-100 resolution,  156  Hz PWM frequency,  32  Hz timer frequency (prescaler 9)
	{  255, 8 }, // 0-255 resolution,  122  Hz PWM frequency,  65  Hz timer frequency (prescaler 8)
    { 1000, 5 }, // 0-1000 resolution, 125  Hz PWM frequency, 500  Hz timer frequency (prescaler 5)
    {  100, 2 }, // 0-100 resolution,   20 kHz PWM frequency,   4 MHz timer frequency (prescaler 2)
    {  255, 0 }, // 0-266 resolution,   31 kHz PWM frequency,  16 MHz timer frequency (prescaler 0)
};

void
PWM_IRQHandler(void)
{
    static uint32_t i;

    PWM_TIMER->EVENTS_COMPARE[3] = 0;
    PWM_TIMER->INTENCLR = 0xFFFFFFFF;

    for(i = 0; i <= _chans; i++) {
        if(pwm_next_value[i] == 0) {
            nrf_gpiote_unconfig(pwm_gpiote_channel[i]);
            nrf_gpio_pin_write(pwm_io_ch[i], 0);
            pwm_running[i] = 0;
        } else if (pwm_next_value[i] >= pwm_max_value) {
            nrf_gpiote_unconfig(pwm_gpiote_channel[i]);
            nrf_gpio_pin_write(pwm_io_ch[i], 1); 
            pwm_running[i] = 0;
        } else {
            PWM_TIMER->CC[i] = pwm_next_value[i] * 2;
            if(!pwm_running[i]) {
                nrf_gpiote_task_config(pwm_gpiote_channel[i], pwm_io_ch[i], NRF_GPIOTE_POLARITY_TOGGLE, NRF_GPIOTE_INITIAL_VALUE_HIGH);  
                pwm_running[i] = 1;
            }
        }
    }

    PWM_TIMER->SHORTS = TIMER_SHORTS_COMPARE3_CLEAR_Msk;
    PWM_TIMER->TASKS_START = 1;
}

int32_t
ppi_enable_first_available_channel(volatile uint32_t *event_ptr, volatile uint32_t *task_ptr)
{
    uint32_t channel_num;
    uint32_t chen;
    uint32_t err;

    err = sd_ppi_channel_enable_get(&chen);
    APP_ERROR_CHECK(err);
    DEBUG("PPI chan msk: ", chen);
    // Traverse the total list of channels
    for(channel_num = 0; channel_num < 16; channel_num++) {
        // Look for a channel that is not set
        //if((NRF_PPI->CHEN & (1 << channel_num)) == 0) {
        if((chen & (1 << channel_num)) == 0) {
            // When a free channel is found, exit the loop
            break;
        }
    }

    if(channel_num >= 16) {
        // If the loop reaches the end no channels are free, and we exit returning -1
        return -1;
    } else {
        // Otherwise we configure the channel and return the channel number
        err = sd_ppi_channel_assign(channel_num, event_ptr, task_ptr);
         APP_ERROR_CHECK(err);
        //*(&(NRF_PPI->CH0_EEP) + (channel_num * 2)) = (uint32_t)event_ptr;
        //*(&(NRF_PPI->CH0_TEP) + (channel_num * 2)) = (uint32_t)task_ptr;    
        //NRF_PPI->CHENSET = (1 << channel_num);   
        err = sd_ppi_channel_enable_set(1<<channel_num);
         APP_ERROR_CHECK(err);
         DEBUG("new PPI: ", channel_num);
         return channel_num;
    }
}

uint32_t
pwm_init(uint32_t num_channels, uint32_t *gpios, PWM_Mode mode) {
	uint32_t i;
    uint32_t err;

	if (num_channels >= PWM_Num_Channels)
		return 1;

	_chans = num_channels;

	if (mode >= PWM_Mode_Invalid)
		return 2;

	_mode = mode;

	for (i=0; i <= num_channels; i++) {
		pwm_io_ch[i] = gpios[i];
		nrf_gpio_cfg_output(gpios[i]);
    	pwm_running[i] = 0;
	}

	PWM_TIMER->TASKS_CLEAR = 1;
	PWM_TIMER->BITMODE = TIMER_BITMODE_BITMODE_16Bit;

	PWM_TIMER->PRESCALER = _configs[mode].prescaler;
	pwm_max_value = _configs[mode].max_value;

	PWM_TIMER->CC[3] = pwm_max_value*2;
	PWM_TIMER->MODE = TIMER_MODE_MODE_Timer;
    PWM_TIMER->SHORTS = TIMER_SHORTS_COMPARE3_CLEAR_Msk;
    PWM_TIMER->EVENTS_COMPARE[0] = 0;
    PWM_TIMER->EVENTS_COMPARE[1] = 0;
    PWM_TIMER->EVENTS_COMPARE[2] = 0;
    PWM_TIMER->EVENTS_COMPARE[3] = 0;

    for(i = 0; i <= num_channels; i++) {
        ppi_enable_first_available_channel(&PWM_TIMER->EVENTS_COMPARE[i], &NRF_GPIOTE->TASKS_OUT[pwm_gpiote_channel[i]]);
        ppi_enable_first_available_channel(&PWM_TIMER->EVENTS_COMPARE[3], &NRF_GPIOTE->TASKS_OUT[pwm_gpiote_channel[i]]);        
    }

    //NVIC_SetPriority(PWM_IRQn, 3);
    err = sd_nvic_SetPriority(PWM_IRQn, 1);
    APP_ERROR_CHECK(err);
    err = sd_nvic_EnableIRQ(PWM_IRQn);
    APP_ERROR_CHECK(err);
    //NVIC_EnableIRQ(PWM_IRQn);
    PWM_TIMER->TASKS_START = 1;

	return 0;
}

uint32_t
pwm_set_value(uint32_t channel, uint32_t value) {
	if (_mode >= PWM_Mode_Invalid)
		return 1;

	if (_chans >= PWM_Num_Channels)
		return 2;

	if (channel > _chans)
		return 3;

	pwm_next_value[channel] = value;
    PWM_TIMER->EVENTS_COMPARE[3] = 0;
    PWM_TIMER->SHORTS = TIMER_SHORTS_COMPARE3_CLEAR_Msk | TIMER_SHORTS_COMPARE3_STOP_Msk;
    PWM_TIMER->INTENSET = TIMER_INTENSET_COMPARE3_Msk;    
    PWM_TIMER->TASKS_START = 1;

    return 0;
}

const uint8_t sin_table[] = {0, 0,1,2,4,6,9,12,16,20,24,29,35,40,   46, 53, 59, 66, 74, 81, 88, 96, 104,112,120,128,136,144,152,160,168,175,182,190,197,203,210,216,221,227,
           232,236,240,244,247,250,252,254,255,255,255,255,255,254,252,250,247,244,240,236,232,227,221,216,210,203,197,190,182,175,168,160,152,144,136,128,120,112,104,
           96,88,81,74,66,59,   53, 46, 40, 35, 29,24,  20, 16, 12, 9,  6,  4,  2,1,0};
    
void
pwm_test() {
    uint32_t err_code;
    uint32_t counter = 0;

    uint32_t gpios[] = {
        GPIO_HRS_PWM_G
    };

    err_code = pwm_init(PWM_1_Channel, gpios, PWM_Mode_122Hz_255);
    APP_ERROR_CHECK(err_code);

    nrf_gpio_cfg_output(GPIO_3v3_Enable);
    nrf_gpio_pin_set(GPIO_3v3_Enable);

    while(1) {
        err_code = pwm_set_value(PWM_1_Channel, sin_table[counter]);
        APP_ERROR_CHECK(err_code);
        if (++counter >= 100)
            counter = 0; 
        nrf_delay_us(8000);
    }
}
