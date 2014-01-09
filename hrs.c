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

typedef void(*hrs_adc_callback)(uint8_t);

void
null_cb(uint8_t val __attribute__((unused))) {
}

static hrs_adc_callback adc_callback = &null_cb;

static uint16_t buckets[NUM_BUCKETS];
static volatile uint32_t measure_count = 0;
static uint32_t measure_limit = 0;
static uint32_t ppi_chan = 0;
static uint32_t conf_done = 2;

static uint8_t discard_threshold = UCHAR_MAX;
static volatile bool discard_threshold_passed = false;

#define MAX_SAMPLE_COUNT 1500

static uint8_t buffer[MAX_SAMPLE_COUNT];
static uint16_t buf_lvl;

#define HRS_TIMER               NRF_TIMER1
#define HRS_IRQHandler          TIMER1_IRQHandler
#define HRS_IRQn                TIMER1_IRQn

void HRS_IRQHandler() {
    //PRINTS("HRS_TIMER");
    HRS_TIMER->EVENTS_COMPARE[0] = 0;
    HRS_TIMER->INTENCLR = 0xFFFFFFFF;
    HRS_TIMER->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Msk;
    HRS_TIMER->CC[0] = 2500;
    HRS_TIMER->TASKS_START = 1;
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
    sd_nvic_SetPriority(HRS_IRQn, 3);
    //NVIC_SetPriority(HRS_IRQn, 3);
    //NVIC_EnableIRQ(HRS_IRQn);
    sd_nvic_EnableIRQ(HRS_IRQn);
    HRS_TIMER->TASKS_START = 1;

    ppi_chan = ppi_enable_first_available_channel(&HRS_TIMER->EVENTS_COMPARE[0], &NRF_ADC->TASKS_START);
    APP_ERROR_CHECK(ppi_chan == -1);
    //NRF_PPI->CHENSET = (1 << ppi_chan);
}

static void
hrs_adc_start() {
    // Enable the ADC interrupt, and set the priority to 1
    //NVIC_SetPriority(ADC_IRQn, 1);
    //NVIC_EnableIRQ(ADC_IRQn);
    sd_nvic_SetPriority(ADC_IRQn, 1);
    sd_nvic_EnableIRQ(ADC_IRQn);
    NRF_ADC->ENABLE = 1;

    // Start the ADC
    //NRF_ADC->TASKS_START = 1;
}

static void
hrs_adc_stop() {
    NRF_ADC->TASKS_STOP = 1;
    sd_nvic_DisableIRQ(ADC_IRQn);
    //NVIC_DisableIRQ(ADC_IRQn);
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

void
hrs_debug_cb(uint8_t val) {
    DEBUG("", val);
}

void
hrs_calibration_cb(uint8_t val) {
    //if (val != 0)
    //DEBUG("", val);

    if (val < BUCKET_LVL)
        ++buckets[0];
    else if (val < BUCKET_LVL*2)
        ++buckets[1];
    else if (val < BUCKET_LVL*3)
        ++buckets[2];
    else if (val < BUCKET_LVL*4)
        ++buckets[3];
    else if (val < BUCKET_LVL*5)
        ++buckets[4];
    else if (val < BUCKET_LVL*6)
        ++buckets[5];
    else if (val < BUCKET_LVL*7)
        ++buckets[6];
    else if (val < BUCKET_LVL*8)
        ++buckets[7];
    else if (val < BUCKET_LVL*9)
        ++buckets[8];
    else if (val < BUCKET_LVL*10)
        ++buckets[9];
    else if (val < BUCKET_LVL*11)
        ++buckets[10];
    else if (val < BUCKET_LVL*12)
        ++buckets[11];
    else if (val < BUCKET_LVL*13)
        ++buckets[12];
    else
        ++buckets[13];
/*
    if (++measure_count > measure_limit) {
        PRINT_HEX(&buckets[0], 4);
        PRINT_HEX(&buckets[1], 4);
        PRINT_HEX(&buckets[2], 4);
        PRINT_HEX(&buckets[3], 4);
        PRINTS("\r\n");
        memset(buckets, 0, NUM_BUCKETS*sizeof(uint32_t));
        measure_count = 0;
    }*/
}


uint8_t *
hrs_get_buffer() {
    return buffer;
}

void
hrs_threshold_cb(uint8_t val) {
    if (val <= discard_threshold) {
	discard_threshold_passed = true;
    } else {
        DEBUG("Over threshold: ", val);
    }
}

void
hrs_test_cb(uint8_t val) {
    if (measure_count < measure_limit)
        buffer[buf_lvl++] = val;
}

void
hrs_calibrate(uint8_t power_lvl_min, uint8_t power_lvl_max, uint16_t delay, uint16_t samples, hrs_send_data_cb data_send) {
    uint32_t i;

    APP_ERROR_CHECK(data_send !=NULL);

    DEBUG("pwr min ", power_lvl_min);
    DEBUG("pwr max ", power_lvl_max);
    DEBUG("samples ", samples);

    for(i = power_lvl_min; i <= power_lvl_max; i++) {
        hrs_run_test(i, delay, samples, 0);
        data_send(buffer, samples);
    }
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

void hrs_run_test2(hrs_parameters_t parameters) {
    uint32_t err_code;
    uint32_t i;
    uint32_t gpios[] = {
	    GPIO_HRS_PWM_G1,
#ifndef ONE_HRS_LED
        GPIO_HRS_PWM_G2
#endif
    };

    APP_ERROR_CHECK(parameters.samples > sizeof(buffer));

    buf_lvl = 0;

    // enable the HRS sensor
    hrs_sensor_enable();

    if (conf_done == 2) {
        // one-time init
        PRINTS("HRS_RUN INIT");

        // drive PWM at 20kHz and range is 0-100 for intensity
#ifdef ONE_HRS_LED
        err_code = pwm_init(PWM_1_Channel, gpios, PWM_Mode_20kHz_100);
#else
        err_code = pwm_init(PWM_2_Channels, gpios, PWM_Mode_20kHz_100);
#endif
        APP_ERROR_CHECK(err_code);

        // configure ADC
        hrs_adc_conf();
        hrs_adc_start();
        conf_done = 1;
    }

    DEBUG("HRS power ", parameters.power_level);

    // turn on LED at specified brightness
    err_code = pwm_set_value(PWM_Channel_1, (uint32_t)parameters.power_level);
    APP_ERROR_CHECK(err_code);
#ifndef ONE_HRS_LED
    err_code = pwm_set_value(PWM_Channel_2, (uint32_t)parameters.power_level);
    APP_ERROR_CHECK(err_code);
#endif

    // setup counters for limiting
    measure_count = 0;
    measure_limit = parameters.samples;

    // wait for sensor output to settle
    nrf_delay_ms(parameters.delay);

    // enable PPI
    err_code = sd_ppi_channel_enable_set(1<<ppi_chan);
    APP_ERROR_CHECK(err_code);

    // discard the first N samples, where N = parameters.discard_samples
    adc_callback = &null_cb;
    for(i = 0; i < parameters.discard_samples; i++) {
	__WFE();
	watchdog_pet();
    }
    DEBUG("Discarded number of samples: ", parameters.discard_samples);

    // wait for the first sample to pass the threshold check
    discard_threshold_passed = false;
    discard_threshold = parameters.discard_threshold;
    adc_callback = &hrs_threshold_cb;
    for(;;) {
        __WFE();
    	watchdog_pet();

    	if(discard_threshold_passed) {
    	    discard_threshold_passed = false;
    	    break;
    	}
    }

    // read samples and wait for completion
    adc_callback = &hrs_test_cb;
    while (measure_count < measure_limit) {
        __WFE();
    	watchdog_pet();
    }

    // disable PPI
    err_code = sd_ppi_channel_enable_clr(1<<ppi_chan);
    APP_ERROR_CHECK(err_code);

    // turn off LED
    if (!parameters.keep_the_lights_on) {
        pwm_set_value(PWM_Channel_1, 0);
#ifndef ONE_HRS_LED
        pwm_set_value(PWM_Channel_2, 0);
#endif
    }
}

#define MDELAY  4000
/*
uint32_t
hrs_calibrate() {
    uint32_t err_code;
    uint32_t i;
    uint32_t gpios[] = {
        GPIO_HRS_PWM_G
    };

    // we should probably kill or pause the softdevice here



    adc_callback = &hrs_calibration_cb;

    // enable the HRS sensor
    hrs_sensor_enable();

    // drive PWM at 20kHz and range is 0-100 for intensity
    err_code = pwm_init(PWM_Channel_1, gpios, PWM_Mode_20kHz_100);
    APP_ERROR_CHECK(err_code);

// configure ADC
    hrs_adc_conf();
    hrs_adc_start();



    uint32_t best_fit = 0;
    uint8_t minima = 100;
#if 0
    for (i=0; i <= 14; i+=1) {
        measure_count = 0;
        measure_limit = 200; // whatever sampling rate we're using * 1.5

        err_code = pwm_set_value(PWM_Channel_1, i);
        APP_ERROR_CHECK(err_code);
        DEBUG("Power lvl ", i);

        // wait for sensor to stabilize from the light change
        // TODO: watch for zero
        nrf_delay_ms(MDELAY);

        // start samples
        //hrs_adc_start();
        memset(buckets, 0, sizeof(buckets));
        // enable PPI
        NRF_PPI->CHENSET = (1 << ppi_chan);

        // wait for completion
        while (measure_count < measure_limit) {
            __WFE();
        }

        // disable PPI
        NRF_PPI->CHENCLR = (1 << ppi_chan);

        // stop ADC
        //hrs_adc_stop();

        // analyze readings
        if (buckets[0] < minima) {
            minima = buckets[0];
            best_fit = i;
        }

        PRINT_HEX(buckets, sizeof(buckets));
        PRINTS("\r\n");


    }
    DEBUG("Best fit: ", best_fit);
    DEBUG("Minima: ", minima);

    uint32_t temp = best_fit;
    best_fit = 0;
    minima = 100;

    for (i=temp-1; i < temp+2; i++) {
        measure_count = 0;
        measure_limit = 200; // whatever sampling rate we're using * 1.5

        err_code = pwm_set_value(PWM_Channel_1, i);
        APP_ERROR_CHECK(err_code);
        //DEBUG("Power lvl ", i);

        // wait for sensor to stabilize from the light change
        // TODO: watch for zero
        nrf_delay_ms(MDELAY);

        // start samples
        //hrs_adc_start();
        memset(buckets, 0, sizeof(buckets));
        // enable PPI
        NRF_PPI->CHENSET = (1 << ppi_chan);

        // wait for completion
        while (measure_count < measure_limit) {
            __WFE();
        }

        // disable PPI
        NRF_PPI->CHENCLR = (1 << ppi_chan);

        // stop ADC
        //hrs_adc_stop();

        // analyze readings
        if (buckets[0] < minima) {
            minima = buckets[0];
            best_fit = i;
        }
    }
    DEBUG("Best fit: ", best_fit);
    DEBUG("Minima: ", minima);
    if (temp != best_fit) {
        DEBUG("cal diverged from ", temp);
        DEBUG("to ", best_fit);
    }
#endif
    err_code = pwm_set_value(PWM_Channel_1, 9);
    APP_ERROR_CHECK(err_code);
    adc_callback = &hrs_debug_cb;
    measure_count = 0;
    measure_limit = 500;
    nrf_delay_ms(MDELAY);

    //for (i=0; i < 200; i++) {
        // enable PPI
        NRF_PPI->CHENSET = (1 << ppi_chan);

        // wait for completion
        while (measure_count < measure_limit) {
            __WFE();
        }

        // disable PPI
        NRF_PPI->CHENCLR = (1 << ppi_chan);

    err_code = pwm_set_value(PWM_Channel_1, 0xa);
    APP_ERROR_CHECK(err_code);
    adc_callback = &hrs_debug_cb;
    measure_count = 0;
    measure_limit = 500;
    nrf_delay_ms(MDELAY);

    //for (i=0; i < 200; i++) {
        // enable PPI
        NRF_PPI->CHENSET = (1 << ppi_chan);

        // wait for completion
        while (measure_count < measure_limit) {
            __WFE();
        }

        // disable PPI
        NRF_PPI->CHENCLR = (1 << ppi_chan);

    //}
    pwm_set_value(PWM_Channel_1, 0);
}*/

static void
hrs_test_callback(uint8_t val) {

    if (val < BUCKET_LVL)
        ++buckets[0];
    else if (val < BUCKET_LVL*2)
        ++buckets[1];
    else if (val < BUCKET_LVL*3)
        ++buckets[2];
    else if (val < BUCKET_LVL*4)
        ++buckets[3];
    else if (val < BUCKET_LVL*5)
        ++buckets[4];
    else if (val < BUCKET_LVL*6)
        ++buckets[5];
    else if (val < BUCKET_LVL*7)
        ++buckets[6];
    else if (val < BUCKET_LVL*8)
        ++buckets[7];
    else if (val < BUCKET_LVL*9)
        ++buckets[8];
    else if (val < BUCKET_LVL*10)
        ++buckets[9];
    else if (val < BUCKET_LVL*11)
        ++buckets[10];
    else if (val < BUCKET_LVL*12)
        ++buckets[11];
    else if (val < BUCKET_LVL*13)
        ++buckets[12];
    else
        ++buckets[13];

    //PRINT_HEX(&tmp, 1);
    //PRINTC(' ');

    if (++measure_count > measure_limit) {
        PRINT_HEX(&buckets[0], 4);
        PRINT_HEX(&buckets[1], 4);
        PRINT_HEX(&buckets[2], 4);
        PRINT_HEX(&buckets[3], 4);
        PRINTS("\r\n");
        memset(buckets, 0, NUM_BUCKETS*sizeof(uint32_t));
        measure_count = 0;
    }
}

void
ADC_IRQHandler(void)
{
    if(!NRF_ADC->EVENTS_END)
        PRINTS("ARGH");


    // Read the ADC result, and dispatch the callback
    adc_callback(NRF_ADC->RESULT);

    // Clear the END event
    NRF_ADC->EVENTS_END = 0;

    ++measure_count;

// Prevent further sampling
    NRF_ADC->TASKS_STOP = 1;
    // for free-sampling, trigger a new sample cycle
    //nrf_delay_ms(8);
    //NRF_ADC->TASKS_START = 1;
}

void
adc_test() {
    uint32_t err_code;

    hrs_adc_conf();

    hrs_sensor_enable();

    // configure LED
    uint32_t gpios[] = {
        GPIO_HRS_PWM_G1
    };

    err_code = pwm_init(PWM_1_Channel, gpios, PWM_Mode_20kHz_100);
    APP_ERROR_CHECK(err_code);
    err_code = pwm_set_value(PWM_Channel_1, 66);
    APP_ERROR_CHECK(err_code);

    hrs_adc_start();

    while (true)
    {
        __WFE();
    }
}
