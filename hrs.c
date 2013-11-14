// vi:sw=4:ts=4

#include <app_error.h>
#include <device_params.h>
#include <hrs.h>
#include <limits.h>
#include <nrf.h>
#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <pwm.h>
#include <stdint.h>
#include <string.h>
#include <util.h>
#include <nrf_soc.h>
#include <watchdog.h>

static hrs_parameters_t hrs_parameters;

static uint32_t ppi_chan = -1;

typedef enum {
	HRS_STATE_START,
	HRS_STATE_DISCARD,
	HRS_STATE_THRESHOLD,
	HRS_STATE_READING,
	HRS_STATE_FINISHED,
} hrs_state_t;

static hrs_state_t hrs_state = HRS_STATE_START;

#define MAX_SAMPLE_COUNT (100*50) // 100 samples/second * 60 seconds

static uint8_t buffer[MAX_SAMPLE_COUNT];

#define HRS_TIMER               NRF_TIMER1
#define HRS_IRQHandler          TIMER1_IRQHandler
#define HRS_IRQn                TIMER1_IRQn

void HRS_IRQHandler() {
    HRS_TIMER->EVENTS_COMPARE[0] = 0;
    HRS_TIMER->INTENCLR = 0xFFFFFFFF;
    HRS_TIMER->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Msk;
    HRS_TIMER->CC[0] = 2500;
    HRS_TIMER->TASKS_START = 1;

	PRINTS("HRS_IRQHandler\r\n");
}

static void
hrs_adc_conf() {
    // Configure the ADC
    NRF_ADC->CONFIG = ADC_CONFIG_RES_8bit << ADC_CONFIG_RES_Pos | \
                    ADC_CONFIG_INPSEL_AnalogInputNoPrescaling << ADC_CONFIG_INPSEL_Pos | \
                    ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos | \
                    HRS_ADC << ADC_CONFIG_PSEL_Pos;
    NRF_ADC->ENABLE = 1;
    NRF_ADC->INTENSET = ADC_INTENSET_END_Msk;

    // setup a PPI channel to trigger ADC
    HRS_TIMER->TASKS_CLEAR = 1;
    HRS_TIMER->BITMODE = TIMER_BITMODE_BITMODE_16Bit;

    HRS_TIMER->PRESCALER = 6;
    HRS_TIMER->MODE = TIMER_MODE_MODE_Timer;
    HRS_TIMER->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Msk;
    HRS_TIMER->EVENTS_COMPARE[0] = 0;
    HRS_TIMER->EVENTS_COMPARE[1] = 0;
    HRS_TIMER->EVENTS_COMPARE[2] = 0;
    HRS_TIMER->EVENTS_COMPARE[3] = 0;
    HRS_TIMER->CC[0] = 2500; // sample rate = 100Hz; 16MHz / 2^6 = 250kHz / 100Hz = 2500

    sd_nvic_SetPriority(HRS_IRQn, NRF_APP_PRIORITY_LOW);
    sd_nvic_EnableIRQ(HRS_IRQn);

    HRS_TIMER->TASKS_START = 1;

    ppi_chan = ppi_enable_first_available_channel(&HRS_TIMER->EVENTS_COMPARE[0], &NRF_ADC->TASKS_START);
    APP_ERROR_CHECK(ppi_chan == -1);
    //NRF_PPI->CHENSET = (1 << ppi_chan);
}

static void
hrs_adc_start()
{
    NRF_ADC->ENABLE = 1;

    sd_nvic_SetPriority(ADC_IRQn, NRF_APP_PRIORITY_HIGH);
    sd_nvic_EnableIRQ(ADC_IRQn);

	NRF_ADC->TASKS_START = 1;
}

static void
hrs_adc_stop() {
	NRF_ADC->TASKS_START = 0;

    //sd_nvic_DisableIRQ(ADC_IRQn);

    NRF_ADC->ENABLE = 0;
}

static void
hrs_sensor_enable() {
    nrf_gpio_cfg_output(GPIO_3v3_Enable);
    nrf_gpio_pin_set(GPIO_3v3_Enable);
}

static void
hrs_sensor_disable() {
    nrf_gpio_pin_clear(GPIO_3v3_Enable);
}

#define MAX_SAMPLE_COUNT (100*50) // 100 samples/second * 60 seconds

static uint8_t buffer[MAX_SAMPLE_COUNT];

uint8_t *
get_hrs_buffer() {
    return buffer;
}

void
hrs_run_test(uint8_t power_lvl, uint16_t delay, uint16_t samples, bool keep_the_lights_on) {
    hrs_parameters_t parameters = {
		.power_level = power_lvl,
		.delay = delay,
		.samples = samples,
		.discard_samples = 200,
		.discard_threshold = UCHAR_MAX,
		.keep_the_lights_on = keep_the_lights_on,
    };

    hrs_run_test2(parameters);
}

static void
hrs_preflight()
{
    uint32_t err_code;
    uint32_t gpios[] = {
	    GPIO_HRS_PWM_G1,
#ifndef ONE_HRS_LED
        GPIO_HRS_PWM_G2
#endif
    };

    APP_ERROR_CHECK(hrs_parameters.samples > sizeof(buffer));

    // enable the HRS sensor
    hrs_sensor_enable();

	static bool conf_done;

    if (!conf_done) {
        // one-time init
        PRINTS("HRS_RUN INIT\r\n");

        // drive PWM at 20kHz and range is 0-100 for intensity
#ifdef ONE_HRS_LED
        err_code = pwm_init(PWM_1_Channel, gpios, PWM_Mode_20kHz_100);
#else
        err_code = pwm_init(PWM_2_Channels, gpios, PWM_Mode_20kHz_100);
#endif
        APP_ERROR_CHECK(err_code);

        // configure ADC
        hrs_adc_conf();

        conf_done = true;
    }

    DEBUG("HRS power ", hrs_parameters.power_level);

    // turn on LED at specified brightness
    err_code = pwm_set_value(PWM_Channel_1, (uint32_t)hrs_parameters.power_level);
    APP_ERROR_CHECK(err_code);
#ifndef ONE_HRS_LED
    err_code = pwm_set_value(PWM_Channel_2, (uint32_t)hrs_parameters.power_level);
    APP_ERROR_CHECK(err_code);
#endif

    // enable PPI
    err_code = sd_ppi_channel_enable_set(1<<ppi_chan);
    APP_ERROR_CHECK(err_code);

	hrs_adc_start();

	hrs_state = HRS_STATE_START;
}

static void
hrs_postflight()
{
    uint32_t err_code;

	watchdog_pet();

	TRACE();

	hrs_adc_stop();

	TRACE();

    // disable PPI
    // err_code = sd_ppi_channel_enable_clr(1<<ppi_chan);
    // APP_ERROR_CHECK(err_code);

    // turn off LED
	DEBUG("Keep lights on? ", hrs_parameters.keep_the_lights_on);

    if (!hrs_parameters.keep_the_lights_on) {
        pwm_set_value(PWM_Channel_1, 0);
#ifndef ONE_HRS_LED
        pwm_set_value(PWM_Channel_2, 0);
#endif
    }
}

static void
adc_callback(uint8_t val) {
	static uint32_t measure_count;
	static uint16_t buf_lvl;

	switch(hrs_state) {
	case HRS_STATE_START:
		measure_count = 0;
		buf_lvl = 0;

		hrs_state = HRS_STATE_DISCARD;

		// Fallthrough
	case HRS_STATE_DISCARD:
		if(measure_count <= hrs_parameters.discard_samples) {
			break;
		} else {
			DEBUG("Discarded number of samples: ", measure_count);

			measure_count = 0;
			hrs_state = HRS_STATE_THRESHOLD;

			// Fallthrough
		}
	case HRS_STATE_THRESHOLD:
		if(val > hrs_parameters.discard_threshold) {
			DEBUG("Over threshold: ", val);

			break;
		} else {
			DEBUG("Discarded sample count due to threshold: ", measure_count);

			measure_count = 0;
			hrs_state = HRS_STATE_READING;

			// Fallthrough
		}
	case HRS_STATE_READING:
		if (measure_count < hrs_parameters.samples) {
			buffer[buf_lvl++] = val;

#ifdef DEBUG
			if(measure_count % 20 == 0) {
				DEBUG("Read samples: ", measure_count);
			}
#endif

			break;
		} else {
			PRINTS("Finished HRS reading\r\n");
			hrs_state = HRS_STATE_FINISHED;

			// Fallthrough
		}
	case HRS_STATE_FINISHED:
		PRINTS("HRS finished\r\n");

		hrs_postflight();
		return;
	}

	++measure_count;

	watchdog_pet();
}

void
hrs_run_test2(hrs_parameters_t parameters) {
	hrs_parameters = parameters;

	hrs_preflight();
}

#define MDELAY  4000

void
ADC_IRQHandler(void)
{
    if(!NRF_ADC->EVENTS_END) {
        PRINTS("ARGH");
	}

    adc_callback(NRF_ADC->RESULT);

	NRF_ADC->EVENTS_END = 0;
    NRF_ADC->TASKS_STOP = 1;
}

// TODO: use hrs_parameters.delay
