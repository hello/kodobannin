#include <pwm.h>
#include <nrf.h>
#include <nrf_gpiote.h>
#include <nrf_gpio.h>

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
    // Traverse the total list of channels
    for(channel_num = 0; channel_num < 16; channel_num++) {
        // Look for a channel that is not set
        if((NRF_PPI->CHEN & (1 << channel_num)) == 0) {
            // When a free channel is found, exit the loop
            break;
        }
    }

    if(channel_num >= 16) {
        // If the loop reaches the end no channels are free, and we exit returning -1
        return -1;
    } else {
        // Otherwise we configure the channel and return the channel number
        *(&(NRF_PPI->CH0_EEP) + (channel_num * 2)) = (uint32_t)event_ptr;
        *(&(NRF_PPI->CH0_TEP) + (channel_num * 2)) = (uint32_t)task_ptr;    
        NRF_PPI->CHENSET = (1 << channel_num);   
        return channel_num;
    }
}

uint32_t
pwm_init(uint32_t num_channels, uint32_t *gpios, PWM_Mode mode) {
	uint32_t i;

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

    NVIC_SetPriority(PWM_IRQn, 3);
    NVIC_EnableIRQ(PWM_IRQn);
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
