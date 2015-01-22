#include <stdlib.h>

#include <app_timer.h>

#include "app.h"
#include "util.h"
#include "nrf_gpio.h"

#include "led.h"
#include "pwm.h"
#include "battery.h"


static __INLINE void _led_gpio_cfg_open_drain(uint32_t pin_number)
{
    /*lint -e{845} // A zero has been given as right argument to operator '|'" */
    NRF_GPIO->PIN_CNF[pin_number] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) // no edge detect
                                  | (GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos)     // nfet pull down only
                                  | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)   // no resistor pull
                                  | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos)  // no input
                                  | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);      // output only
}

// [0]   Vbat (internal resistance reference)
// [1]   Vbat (after weak pullup load enabled)
// [2]   Vbat (after led's enabled)
// [3]   Vbat (after led display completed)

static uint8_t _led_index; // 0 off, 1 on, 2 warm, 3 play
static adc_t _led_bat_ref[4]; // Vbat (pre/during/post load)
static adc_t _led_bat_rel[4]; // Vbat (pre/during/post load)
static adc_t _led_bat_off[4]; // Vbat (pre/during/post load)
static adc_t _led_boost[4]; // Vrgb (offset, passive, active)
static adc_t _led_sense[4]; // Vlad (zero, passive, active)

// static Vbat / Vrgb / Vlad [8] for each led color

static adc_t _led_red[4]; // Vbat/Vrgb/Vlad during led_set sequence
static adc_t _led_grn[4];
static adc_t _led_blu[4];

void led_update_battery_status()
{
    uint8_t i;
    uint32_t result, value;

    for (i=0;i<4;i++) { // battery reference at start
      result = _led_bat_ref[i];
      value = result/256;
      PRINT_BYTE(&value, 1);
      PRINT_HEX(&result, 1);
    } PRINTS("\r\n");

    for (i=0;i<4;i++) { // adc offset voltage
      result = _led_boost[i];
      value = result/256;
      PRINT_BYTE(&value, 1);
      PRINT_HEX(&result, 1);
    } PRINTS("\r\n");

    for (i=0;i<4;i++) { // led forward current
      result = _led_sense[i];
      value = result/256;
      PRINT_BYTE(&value, 1);
      PRINT_HEX(&result, 1);
    } PRINTS("\r\n");

    for (i=0;i<4;i++) { // battery after sustaining load
      result = _led_bat_rel[i];
      value = result/256;
      PRINT_BYTE(&value, 1);
      PRINT_HEX(&result, 1);
    } PRINTS("\r\n");
}

void led_init()
{
    uint32_t gpios[1] = {VRGB_ADJUST}; // port to use for pwm dac

    nrf_gpio_pin_set(VLED_VDD_EN); // set pfet gate high
    nrf_gpio_cfg_output(VLED_VDD_EN); // power off boost regualator

    nrf_gpio_pin_clear(VRGB_ENABLE); // set control input low
    nrf_gpio_cfg_output(VRGB_ENABLE); // disable boost regulator enable input

    nrf_gpio_pin_clear(VRGB_SELECT);  // connect from boost resistor divider ( FB set )
    _led_gpio_cfg_open_drain(VRGB_SELECT); // voltage range select ( grn/blu vs red )

    nrf_gpio_pin_set(VRGB_ADJUST); // disconnect from boost adjust filter ( FB trim )
    _led_gpio_cfg_open_drain(VRGB_ADJUST); // dac output w/pwm filter ( 100K / 0.1uF )

    nrf_gpio_pin_set(LED3_ENABLE); // red led off ( open drain )
    _led_gpio_cfg_open_drain(LED3_ENABLE); // nrf_gpio_cfg_output(LED3_ENABLE); // red

    nrf_gpio_pin_set(LED2_ENABLE); // grn led off ( open drain )
    _led_gpio_cfg_open_drain(LED2_ENABLE); // nrf_gpio_cfg_output(LED2_ENABLE); // grn

    nrf_gpio_pin_set(LED1_ENABLE); // blu led off ( open drain )
    _led_gpio_cfg_open_drain(LED1_ENABLE); // nrf_gpio_cfg_output(LED1_ENABLE); // blu

    APP_OK(pwm_init(PWM_1_Channel, gpios, PWM_Mode_32kHz_255));
    pwm_disable(); // config pwm rate and power down
}


void led_all_colors_on()
{
    nrf_gpio_pin_clear(LED3_ENABLE); // red
    nrf_gpio_pin_clear(LED2_ENABLE); // grn
    nrf_gpio_pin_clear(LED1_ENABLE); // blu
}

void led_all_colors_off()
{
    nrf_gpio_pin_set(LED3_ENABLE); // red
    nrf_gpio_pin_set(LED2_ENABLE); // grn
    nrf_gpio_pin_set(LED1_ENABLE); // blu
}

//    off->on ->up ->set->off
// Vbat 0266 0267 0000 0267  before
// Vrgb 010A 01BF 0000 021E  offset
// Vrgb 01EF 01EE 0000 01EA  leds
// Vbat 0265 0264 0000 0267  after  ( 2.9 V )
//  ==>    1    3  <== f(internal resistance)

static adc_t Vref,Vrel,Voff;

// perform adc readings before led driver config transition
static adc_measure_callback_t led_adc_measured(adc_t adc_result, uint16_t adc_count)
{
    adc_t next_adc_input = 0; // 0xFF; // indicate end of adc reading sequence

    switch (adc_count) // 0 (Vmcu) 1 (Vbat) [ 2 (Vrgb) [ 3 (Vlad) [ 4 (Vbat) ]]]
    {
        case 0: // Vmcu (settle) // should never happen, trapped by IRQ
            next_adc_input = LDO_VBAT_ADC_INPUT;
            break;;
        case 1: // Vbat (reference) (passive) (illuminate) (extinguish)
            _led_bat_ref[_led_index] = adc_result; // battery internal resistance
            next_adc_input = LDO_VRGB_ADC_INPUT;
            break;;
        case 2: // Vrgb (offset) (precharge) (current) (discharge)
            _led_boost[_led_index] = adc_result; // rgb led forward current
            next_adc_input = LED_GREEN_SENSE;
            break;;
        case 3: // Vlad (zero) (passive) (current) (discharge)
            _led_sense[_led_index] = adc_result; // rgb led forward current
            next_adc_input = LDO_VBAT_ADC_INPUT;
            break;;
        case 4: // Vbat (zero) (passive) (current) (discharge)
            _led_bat_rel[_led_index] = adc_result; // battery internal resistance
            break;;
    }

    switch (_led_index)
    {
        case 0: // off -> on
            switch (adc_count) // 0 (Vmcu) 1 (Vbat) [ 2 (Vrgb) [ 3 (Vlad) [ 4 (Vbat) ]]]
            {
                case 1: // Vbat(off)
                    battery_module_power_on(); // enable Vbat/Vmcu resistor dividers
                    break;;
                case 2: // Vrgb(offset)
                    Voff = adc_result;
                    break;;
                case 3: //
                    break;;
                case 4: // Vbat(on)
                 // led_power_on(0);
                    break;;
            }
            break;;
        case 1: // on -> warm
            switch (adc_count) // 0 (Vmcu) 1 (Vbat) [ 2 (Vrgb) [ 3 (Vlad) [ 4 (Vbat) ]]]
            {
                case 1: // on -> warm  Vbat(pullup)
                    Vref = adc_result;
                 // battery_set_internal_resistance_cached(adc_result);
                    break;;
                case 2: // warm -> set Vrgb(precharge)
                    break;;
                case 3: //
                    break;;
                case 4: //
                    Vrel = adc_result;
                 // battery_set_internal_resistance_cached(adc_result);
                 // led_warm_up(0);
                    break;;
            }
            break;;
        case 2: // warm -> set
            switch (adc_count) // 0 (Vmcu) 1 (Vbat) [ 2 (Vrgb) [ 3 (Vlad) [ 4 (Vbat) ]]]
            {
                case 1: //
                    break;;
                case 2: //
                    break;;
                case 3: //
                    break;;
                case 4: //
                 // led_run(0);
                    break;;
            }
            break;;
        case 3: // on -> off
            switch (adc_count) // 0 (Vmcu) 1 (Vbat) [ 2 (Vrgb) [ 3 (Vlad) [ 4 (Vbat) ]]]
            {
                case 1: // warm -> off  Vbat
                 // battery_set_voltage_cached(adc_result);
                    break;;
                case 2: //
                    break;;
                case 3: //
                    break;;
                case 4: //
                 // battery_set_internal_resistance_cached(adc_result); 
                    battery_module_power_off(); // disable Vbat/Vmcu resistor dividers
                 // led_power_off(0);
                    break;;
            }
         // battery_set_internal_resistance(adc_result);
            break;;
    }

    if (next_adc_input == 0) {
        PRINTS("=");
        PRINT_BYTE(&adc_count,1);
        PRINTS(":");
        PRINT_BYTE(&next_adc_input,1);
        switch (_led_index) {
            case 0: led_power_on(1);  break;;
            case 1: led_warm_up(1);   break;;
            case 2: // led_run(1);       break;; // ToDo: log V/I led RGB in operation
            case 3: led_power_off(1); break;;
        }
    }
    return next_adc_input;
}

void led_set(int led_channel, int pwmval){

        led_all_colors_off();

        if(pwmval < 0xff && pwmval > 0){
        nrf_gpio_pin_clear(led_channel);
        APP_OK(pwm_set_value(PWM_Channel_1, pwmval));
        }
 // }
}

void led_warm_up(uint8_t mode){ // internal resistance after weak pullup enable
#ifdef PLATFORM_HAS_VLED
    if (mode == 0) { // perform adc readings before warm up config

        _led_index=1;
        battery_measurement_begin(led_adc_measured, 0); // 1); // Vbat(after weak load applied) / Vrgb (passive boost precharge)

    } else {

        PRINTS("===( LED power");
        nrf_gpio_pin_clear(VLED_VDD_EN); // write 0 to enable pfet power control
        PRINTS(" on )==="); // boost regulator powered on
    }
#endif
}

void led_power_on(uint8_t mode) // w/o adc or w/adc reading sequence
{
#ifdef PLATFORM_HAS_VLED
    if (mode == 0) { // perform adc readings before power on config

        _led_index=0;
        battery_measurement_begin(led_adc_measured, 0); // 1); // measure Vbat (reference load) / Vrgb (adc offset boost off)

 // gpio_cfg_d0s1_output_disconnect_pull(LED_RED_SENSE, NRF_GPIO_PIN_PULLUP);
 // gpio_cfg_d0s1_output_disconnect_pull(LED_GREEN, NRF_GPIO_PIN_PULLUP);
 // gpio_cfg_d0s1_output_disconnect_pull(LED_BLUE_SENSE, NRF_GPIO_PIN_PULLUP);

    } else { // default: perform led power on configuration

        PRINTS("\r\n===( LED precharge");
        nrf_gpio_pin_set(VRGB_ENABLE);  // precharge capacitors ( Vrgb / Vpwm )

        PRINTS(" pwm");
        APP_OK(pwm_set_value(PWM_Channel_1, 0xEF)); // set initial Vrgb = Vmcu
        PRINTS(" on )===");
    }
#endif
}

void led_power_off(uint8_t mode)
{
#ifdef PLATFORM_HAS_VLED
    if (mode == 0) { // perform adc readings before power off config

        _led_index=3;
        battery_measurement_begin(led_adc_measured, 0); // 1); // capture Vbat(after led load) / Vrgb(discharge)

    } else { // default: perform led power off configuration

        PRINTS(" LED shutdown"); // boost regulator powered on
        nrf_gpio_pin_clear(VRGB_ENABLE); // boost disabled and then

        PRINTS(" pwm");
        pwm_disable();

        PRINTS(" power"); // boost regulator powered on
        nrf_gpio_pin_set(VLED_VDD_EN); // regulator powered off
        PRINTS(" off )===\r\n"); // boost regulator powered on
    }
#endif
}

uint32_t led_check_reed_switch(void){ // assert if reed switch closed
    uint32_t ret = 0;
#ifdef PLATFORM_HAS_REED
    nrf_gpio_cfg_input(LED_REED_ENABLE, NRF_GPIO_PIN_NOPULL);

    ret = nrf_gpio_pin_read(LED_REED_ENABLE);

    nrf_gpio_pin_set(LED_REED_ENABLE); // red led off ( open drain )
    _led_gpio_cfg_open_drain(LED_REED_ENABLE); // dvt's reed switch
#endif
    return ret;
}
