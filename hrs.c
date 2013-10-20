#include <app_error.h>
#include <device_params.h>
#include <hrs.h>
#include <nrf.h>
#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <pwm.h>
#include <stdint.h>
#include <string.h>
#include <util.h>

typedef void(*hrs_adc_callback)(uint8_t);

void
null_cb(uint8_t val) {
    (val);
}

static hrs_adc_callback adc_callback = &null_cb;

static uint16_t buckets[NUM_BUCKETS];
static uint32_t measure_count = 0;
static uint32_t measure_limit = 0;
static uint32_t ppi_chan = 0;

#define HRS_TIMER               NRF_TIMER1
#define HRS_IRQHandler          TIMER1_IRQHandler
#define HRS_IRQn                TIMER1_IRQn

void HRS_IRQHandler() {
    //PRINTS("HRS_TIMER");
    HRS_TIMER->EVENTS_COMPARE[0] = 0;
    HRS_TIMER->INTENCLR = 0xFFFFFFFF;
    HRS_TIMER->SHORTS = TIMER_SHORTS_COMPARE0_CLEAR_Msk;
    PWM_TIMER->CC[0] = 2500;
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
    NVIC_SetPriority(HRS_IRQn, 3);
    NVIC_EnableIRQ(HRS_IRQn);
    HRS_TIMER->TASKS_START = 1;

    ppi_chan = ppi_enable_first_available_channel(&HRS_TIMER->EVENTS_COMPARE[0], &NRF_ADC->TASKS_START);
    APP_ERROR_CHECK(ppi_chan == -1);
    //NRF_PPI->CHENSET = (1 << ppi_chan);
}

static void
hrs_adc_start() {
    // Enable the ADC interrupt, and set the priority to 1
    NVIC_SetPriority(ADC_IRQn, 1);
    NVIC_EnableIRQ(ADC_IRQn);   

    NRF_ADC->ENABLE = 1;

    // Start the ADC
    //NRF_ADC->TASKS_START = 1;
}

static void
hrs_adc_stop() {
    NRF_ADC->TASKS_STOP = 1;
    NVIC_DisableIRQ(ADC_IRQn);
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
    //if (val != 0) DEBUG(" ", val);
    
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
    err_code = pwm_init(PWM_1_Channel, gpios, PWM_Mode_20kHz_100);
    APP_ERROR_CHECK(err_code);

// configure ADC
    hrs_adc_conf();
    hrs_adc_start();

    

    uint32_t best_fit = 0;
    uint8_t minima = 100;
    for (i=0; i <= 20; i+=1) {
        measure_count = 0;
        measure_limit = 100; // whatever sampling rate we're using * 1.5

        err_code = pwm_set_value(PWM_1_Channel, i);
        APP_ERROR_CHECK(err_code);
        DEBUG("Power lvl ", i);

        // wait for sensor to stabilize from the light change
        // TODO: watch for zero
        nrf_delay_ms(500);

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
        measure_limit = 100; // whatever sampling rate we're using * 1.5

        err_code = pwm_set_value(PWM_1_Channel, i);
        APP_ERROR_CHECK(err_code);
        //DEBUG("Power lvl ", i);

        // wait for sensor to stabilize from the light change
        // TODO: watch for zero
        nrf_delay_ms(500);

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

    err_code = pwm_set_value(PWM_1_Channel, 0xa);
    APP_ERROR_CHECK(err_code);
    adc_callback = &hrs_debug_cb;
    measure_count = 0;
    nrf_delay_ms(500);

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
    pwm_set_value(PWM_1_Channel, 0);
}

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
        GPIO_HRS_PWM_G
    };

    err_code = pwm_init(PWM_1_Channel, gpios, PWM_Mode_20kHz_100);
    APP_ERROR_CHECK(err_code);
    err_code = pwm_set_value(PWM_1_Channel, 66);
    APP_ERROR_CHECK(err_code);

    hrs_adc_start();
    
    while (true)
    {
        __WFE();
    }
}
