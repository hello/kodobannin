#include <stdlib.h>

#include <app_timer.h>

#include "app.h"
#include "util.h"
#include "nrf_gpio.h"

#include "led.h"
#include "pwm.h"


#ifdef PLATFORM_HAS_VLED

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
// [2]   Vbat (after weak pullup load enabled)
// [3]   Vbat (after led display completed)

static uint8_t _led_index; // 0 off, 1 on, 2 warm, 3 play
static adc_t _led_bat_ref[4]; // Vbat (pre/during/post load)
static adc_t _led_bat_off[4]; // Vbat (pre/during/post load)
static adc_t _led_boost[4]; // Vrgb (offset, passive, active)
static adc_t _led_sense[4]; // Vlad (zero, passive, active)

// static Vbat / Vrgb / Vlad [8] for each led color

static adc_t _led_red[4]; // Vbat/Vrgb/Vlad during led_set sequence
static adc_t _led_grn[4];
static adc_t _led_blu[4];

/* void led_battery_status()
{
    uint32_t value, result;

    PRINTS("Battery Voltage: ");
    result = battery_get_voltage_cached();
    value = result/1000;
    PRINT_DEC(&value,1);
    PRINTS(".");
    PRINT_DEC(&result,3);

    PRINTS(" V, ");
    value = battery_get_percent_cached();
    PRINT_DEC(&value,2);

    result = hble_adc_start;
    PRINTS("%, ADC: ");
    value = result/256;
    PRINT_BYTE(&value, 1);
    PRINT_HEX(&result, 1);
} */

void led_set(int led_channel, int pwmval){

 // adc_begin(adc_callback); // capture Vrgb / Vlad for each color

    int offset = 0;
    led_all_colors_off();
    if(led_channel == LED_GREEN_CHANNEL){
        offset = 0x8;
    }
    if(pwmval < 0xff && pwmval - offset > 0){
        nrf_gpio_pin_clear(led_channel);
        APP_OK(pwm_set_value(PWM_Channel_1, pwmval - offset));
    }

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
#ifdef PLATFORM_HAS_VLED
    nrf_gpio_pin_clear(LED3_ENABLE); // red
    nrf_gpio_pin_clear(LED2_ENABLE); // grn
    nrf_gpio_pin_clear(LED1_ENABLE); // blu
#endif
}

void led_all_colors_off()
{
#ifdef PLATFORM_HAS_VLED
    nrf_gpio_pin_set(LED3_ENABLE); // red
    nrf_gpio_pin_set(LED2_ENABLE); // grn
    nrf_gpio_pin_set(LED1_ENABLE); // blu
#endif
}

// perform adc readings before led driver config transition
static adc_measure_callback_t led_adc_callback(adc_t adc_result, uint16_t adc_count)
{
    uint8_t next_adc_input = 0xff; // indicate end of adc reading sequence

    switch (adc_count) // 0 (Vbat) 1 (Vrgb) [ 2 (Vlad) [ 3 (Vbat) ]]
    {
        case 0: // Vbat (reference) (passive) (illuminate) (extinguish)
            _led_bat_ref[_led_index] = adc_result; // battery internal resistance
            next_adc_input = LDO_VRGB_ADC_INPUT;
            break;;
        case 1: // Vrgb (offset) (precharge) (current) (discharge)
            _led_boost[_led_index] = adc_result; // rgb led forward current
            next_adc_input = LED_GREEN_SENSE;
            break;;
        case 2: // Vlad (zero) (passive) (current) (discharge)
            _led_sense[_led_index] = adc_result; // rgb led forward current
            next_adc_input = LDO_VBAT_ADC_INPUT;
            break;;
        case 3: // Vbat (zero) (passive) (current) (discharge)
            _led_bat_rof[_led_index] = adc_result; // battery internal resistance
    }

    _led_index += 1; // inc
    _led_index %= 4; // 0 -> 1 -> 2 -> 3 -> 0 ...

 /* if (_led_index)
    {
        case 0: // off -> on
            switch (adc_count) // 0 (Vbat) 1 (Vrgb) [ 2 (Vlad) [ 3 (Vbat) ]]
            {
                case 0: // off -> on
                 // battery_set_voltage_cached(adc_result);
                    _led_index=++;
                    break;;
                case 1: // on Vrgb(offset)
                    _led_index=0;
                    _led_power_on();
                    break;;
            }
            break;;
        case 1: // on -> warm
            switch (_led_index) // 0 (Vbat) 1 (Vrgb) [ 2 (Vlad) [ 3 (Vbat) ]]
            {
                case 0: // on -> warm  Vbat(pullup)
                 // battery_set_internal_resistance_cached(adc_result);
                    _led_index=++;
                case 1: // warm -> set Vrgb(precharge)
                    _led_index=0;
                    _led_warm_up();
                    break;;
            }
            break;;
        case 2: // warm -> set
            switch (_led_index) // 0 (Vbat) 1 (Vrgb) [ 2 (Vlad) [ 3 (Vbat) ]]
            {
                    _led_index=0;
                    _led_set();
            }
            break;;
        case 3: // on -> off
            switch (_led_index) // 0 (Vbat) 1 (Vrgb) [ 2 (Vlad) [ 3 (Vbat) ]]
            {
                    _led_index=0;
                    _led_power_off();
            }
         // battery_set_internal_resistance(adc_result);
            break;;
    } */

    return next_adc_input;
}

void led_warm_up(){ // return battery state (internal resistance after weak pullup enable)
#ifdef PLATFORM_HAS_VLED
 // adc_begin(adc_callback); // Vbat(after weak load applied) / Vrgb (passive boost precharge)

    PRINTS("===( LED power");
    nrf_gpio_pin_clear(VLED_VDD_EN); // write 0 to enable pfet power control
    PRINTS(" on )==="); // boost regulator powered on
#endif
}

// adc_callback_t _adc_offset_callback()
// if next reading else led_power_on()

// led_power_on_begin() // (uint8_t mode) w/o adc or w/adc reading sequence
// { _begin(callback);

void led_power_on() // (uint8_t mode) w/o adc or w/adc reading sequence
{
#ifdef PLATFORM_HAS_VLED
 // if (mode) { // perform adc readings before power on config
 // adc_begin(adc_callback); // measure Vbat (reference load) / Vrgb (adc offset boost off)

 // gpio_cfg_d0s1_output_disconnect_pull(LED_RED_SENSE, NRF_GPIO_PIN_PULLUP);
 // gpio_cfg_d0s1_output_disconnect_pull(LED_GREEN, NRF_GPIO_PIN_PULLUP);
 // gpio_cfg_d0s1_output_disconnect_pull(LED_BLUE_SENSE, NRF_GPIO_PIN_PULLUP);

 // } else { // default: perform led power on configuration
    PRINTS("\r\n===( LED precharge");
    nrf_gpio_pin_set(VRGB_ENABLE);  // precharge capacitors ( Vrgb / Vpwm )

    PRINTS(" pwm");
    APP_OK(pwm_set_value(PWM_Channel_1, 0xEF)); // set initial Vrgb = Vmcu
    PRINTS(" on )===");
 // }

#endif
}


void led_power_off()
{
#ifdef PLATFORM_HAS_VLED
 // adc_begin(adc_callback); // capture Vbat(after led load) / Vrgb(discharge)

    PRINTS(" LED shutdown"); // boost regulator powered on
    nrf_gpio_pin_clear(VRGB_ENABLE); // boost disabled and then

    PRINTS(" pwm");
    pwm_disable();

    PRINTS(" power"); // boost regulator powered on
    nrf_gpio_pin_set(VLED_VDD_EN); // regulator powered off
    PRINTS(" off )===\r\n"); // boost regulator powered on
#endif
}



#endif
