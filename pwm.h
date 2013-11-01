#pragma once

#include <stdint.h>

// To change the timer used for the PWM library replace the three defines below
#define PWM_TIMER               NRF_TIMER2
#define PWM_IRQHandler          TIMER2_IRQHandler
#define PWM_IRQn                TIMER2_IRQn

// To change the GPIOTE channels used by the different PWM channels, please change the defines below
#define PWM_GPIOTE_CHANNEL0     2
#define PWM_GPIOTE_CHANNEL1     3
#define PWM_GPIOTE_CHANNEL2     0

typedef enum {
	PWM_1_Channel = 0,
	PWM_2_Channels,
	PWM_3_Channels,
	PWM_Num_Channels
} PWM_Channel;

enum {
	PWM_Channel_1 = 0,
	PWM_Channel_2,
};

typedef struct {
	uint32_t max_value;
	uint8_t  prescaler;
} PWM_Config;

typedef enum {
	PWM_Mode_156Hz_100,
    PWM_Mode_122Hz_255,
    PWM_Mode_125Hz_1000,
    PWM_Mode_20kHz_100,
    PWM_Mode_32kHz_255, 
    PWM_Mode_Invalid
} PWM_Mode;

uint32_t pwm_init(uint32_t num_channels, uint32_t *gpios, PWM_Mode mode);
uint32_t pwm_set_value(uint32_t channel, uint32_t value);

// Test func
void pwm_test();
int32_t ppi_enable_first_available_channel(volatile uint32_t *event_ptr, volatile uint32_t *task_ptr);
