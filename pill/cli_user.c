#include "cli_user.h"
#include "util.h"
#include "ant_driver.h"
#include <nrf_soc.h>
#include "message_led.h"
#include "message_imu.h"
#include "message_time.h"
#include "app.h"
#include "battery.h"
#include <ble.h>

static adc_t Vref, Vrel, Voff;
static uint8_t cli_ant_packet_enable; // heartbeat periodic Vbat percentage

static uint8_t cli_battery_level_measured(adc_t adc_result, uint16_t adc_count)
{
    uint32_t value, result;      //  Measured        Actual
 // Battery Voltage  0x0267 - 0x010A +2.914 V,  80 %   2.9 V
    switch (adc_count) { // for each adc reading
            case 1: battery_set_result_cached(adc_result); Vref = adc_result;
                    PRINTS(" 0x");
                    value = adc_result/256;
                    PRINT_BYTE(&value, 1);
                    PRINT_HEX(&adc_result, 1);
                    return LDO_VRGB_ADC_INPUT; break; // for adc offset
            case 2: battery_set_offset_cached(adc_result); Voff = adc_result;
                    PRINTS("- 0x");
                    value = adc_result/256;
                    PRINT_BYTE(&value, 1);
                    PRINT_HEX(&adc_result, 1);
                    return LDO_VBAT_ADC_INPUT; break; // spread print overhead
            case 3: result = battery_set_voltage_cached(adc_result); Vrel = adc_result;
                    PRINTS(" 0x");
                    value = adc_result/256;
                    PRINT_BYTE(&value, 1);
                    PRINT_HEX(&adc_result, 1);
                    PRINTS(" +");
                    value = result/1000;
                    PRINT_DEC(&value);
                    PRINTS(".");
                    PRINT_DEC(&result);
                    PRINTS(" V, ");
                    return LDO_VBAT_ADC_INPUT; break; // spread print overhead
            case 4: result = battery_get_initial_cached(cli_ant_packet_enable);
                    PRINTS(" +");
                    value = result/1000;
                    PRINT_DEC(&value);
                    PRINTS(".");
                    PRINT_DEC(&result);
                    PRINTS(" V, ");
                    result = battery_get_percent_cached();
                    value = (result%3);
                    switch (value) {
                        case 0: PRINTS("+");break; // better than expected
                        case 2: PRINTS("=");break; // nominal 2/3/4 for each 30%
                        case 1: PRINTS("-");break; // worse than expected
                    }
                    PRINT_DEC(&result);
                    if ( Vrel > Vref) {
                        result = Vrel - Vref; 
                        PRINTS("% +");
                    } else {
                        PRINTS("% -");
                        result = Vref - Vrel; 
                    } // estimate of battery internal resistance
                    PRINT_HEX(&result, 1);
                    PRINTS("\r\n");
                    break; // fall thru to end adc reading sequence
    }

 // if (cli_ant_packet_enable) // periodic Vbat percentage
 //     send_heartbeat_packet();

    return 0; // indicate no more adc conversions required
}

/*
 *  ADC    OFFSET  Vbat Measured    Actual
 *0x02CE - 0x010A +4.011 V, 120 %   4.0 V
 *0x028C - 0x010A +3.220 V, 102 %   3.2 V
 *0x0273 - 0x010A +3.013 V, 100 %   3.0 V \
 *0x0267 - 0x010A +2.914 V,  80 %   2.9 V |  normal battery
 *0x025B - 0x010A +2.814 V,  40 %   2.8 V |  operating range
 *0x0243 - 0x010A +2.714 V,  10 %   2.6 V /
 *0x0240 - 0x010A +2.614 V,  05 %   2.4 V
 *0x022B - 0x010A +2.415 V,  00 %   2.2 V
 *0x01FA - 0x010A +2.002 V,  00 %   2.0 V
 */

void cli_update_battery_status()
{
    uint32_t value, result;

    PRINTS("Battery : ");
    result = battery_get_voltage_cached();
    value = result/1000;
    PRINT_DEC(&value);
    PRINTS(".");
    PRINT_DEC(&result);
    PRINTS(" V, ");
    value = battery_get_percent_cached();
    PRINT_DEC(&value);
    PRINTS(" %\r\n");
}

void cli_update_battery_level(uint8_t mode)
{
#ifdef PLATFORM_HAS_VERSION
    cli_ant_packet_enable = mode;
    PRINTS("Battery Voltage "); // resistor divider (523K||215K) to settle
    battery_measurement_begin(cli_battery_level_measured, 0); // Vmcu/Vbat/...
#endif
}


static struct{
    //parent is the reference to the dispatcher 
    MSG_Central_t * parent;
    MSG_CliUserListener_t listener;
}self;
void _send_data_test(void);
static void
_handle_command(int argc, char * argv[]){
    if(argc > 1 && !match_command(argv[0], "echo")){
        PRINTS(argv[1]);
    }
    if( !match_command(argv[0], "advstop")){
        sd_ble_gap_adv_stop();
    }
    if( !match_command(argv[0], "imutest") ){
        self.parent->dispatch( (MSG_Address_t){CLI,0},
                                (MSG_Address_t){IMU, IMU_SELF_TEST},
                                NULL);
    }
    if( !match_command(argv[0], "imuon") ){
        self.parent->loadmod(MSG_IMU_GetBase());
    }
    if( !match_command(argv[0], "mon") ){
        cli_update_battery_level(0); // monitor droop
    }
    if( !match_command(argv[0], "upd") ){
        cli_update_battery_level(1); // measure battery / issue ant packet
    }
    if( !match_command(argv[0], "advstop")){
        sd_ble_gap_adv_stop();
    }
    if( !match_command(argv[0], "free")){
        size_t free_size = xPortGetFreeHeapSize();
        PRINTS("\r\n");
        PRINT_HEX(&free_size, 4);
        PRINTS("\r\n");
    }
    //dispatch message through ANT
    if(argc > 1 && !match_command(argv[0], "ant") ){
        //Create a message object from uart string
        _send_data_test();
        _send_data_test();
/*
 *        MSG_Data_t * data = MSG_Base_AllocateStringAtomic(argv[1]);
 *
 *        if(data){
 *            self.parent->dispatch(  (MSG_Address_t){CLI, 0}, //source address, CLI
 *                                    (MSG_Address_t){ANT,1},//destination address, ANT
 *                                    data);
 *            //release message object after dispatch to prevent memory leak
 *            MSG_Base_ReleaseDataAtomic(data);
 *        }
 */
    }
}

MSG_CliUserListener_t *  Cli_User_Init(MSG_Central_t * parent, void * ctx){
    self.listener.handle_command = _handle_command;
    self.parent = parent;
    return &self.listener;
}
