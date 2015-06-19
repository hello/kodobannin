#include "cli_user.h"
#include "util.h"
#include "ant_driver.h"
#include <nrf_soc.h>
#include "message_led.h"
#include "message_imu.h"
#include "message_time.h"
#include "app.h"
#include "battery.h"
#include "adc_calmgr.h"
#include <ble.h>

adc_t Vref, Vrel, Voff;
uint8_t cli_ant_packet_enable; // heartbeat periodic Vbat percentage

ADC_CalValue_t calue = {
     .pu = 523, // pullup
     .pd = 215, // pulldown
     .slope = 0x1B87, // 7047
     .offset = 0x010A //  226
};

ADC_CalValue_t *p_calue;

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
                    PRINT_DEC(&value,1);
                    PRINTS(".");
                    PRINT_DEC(&result,3);
                    PRINTS(" V, ");
                    return LDO_VBAT_ADC_INPUT; break; // spread print overhead
            case 4: result = battery_get_initial_cached(cli_ant_packet_enable);
                    PRINTS(" +");
                    value = result/1000;
                    PRINT_DEC(&value,1);
                    PRINTS(".");
                    PRINT_DEC(&result,3);
                    PRINTS(" V, ");
                    result = battery_get_percent_cached();
                    if (100 > result && result > 9) {
                        value = (result%3);
                        switch (value) {
                            case 0: PRINTS("+");break; // better than expected
                            case 2: PRINTS("=");break; // nominal 2/3/4 for each 30%
                            case 1: PRINTS("-");break; // worse than expected
                        }
                    } else PRINTS(" ");
                    PRINT_DEC(&result,3);
                    if ( Vrel > Vref) {
                        result = Vrel - Vref; 
                        PRINTS("% +");
                    } else {
                        PRINTS("% -");
                        result = Vref - Vrel; 
                    } // estimate of battery internal resistance
                    PRINT_HEX(&result, 1);
                    return LDO_VMCU_ADC_INPUT; break; // spread print overhead
            case 5: result = battery_get_slope_cached();
                    PRINTS("/ 0x");
                    value = adc_result/256;
                    PRINT_BYTE(&value, 1);
                    PRINT_HEX(&adc_result, 1);
                    PRINTS(" 0x");
                    value = result/256;
                    PRINT_BYTE(&value, 1);
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
    PRINT_DEC(&value,1);
    PRINTS(".");
    PRINT_DEC(&result,3);
    PRINTS(" V, ");
    value = battery_get_percent_cached();
    PRINT_DEC(&value,2);
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

static void
_handle_command(int argc, char * argv[]){
    uint32_t value, result;
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
    if( !match_command(argv[0], "add") ){
     // ADC_CalValue_t *p_value;
        p_calue = &calue;
        ADC_CalMgr_Add(p_calue);
    }
    if( !match_command(argv[0], "put") ){
        ADC_CalMgr_Commit();
    }
    if( !match_command(argv[0], "num") ){
        value = ADC_CalMgr_Count();
        PRINT_DEC(&value,2);
        PRINTS(" \r\n");
    }
    if( !match_command(argv[0], "get") ){
        p_calue = ADC_CalMgr_Query(523,215);
        if (p_calue) {
            value = p_calue->pu;
            PRINT_DEC(&value,4);
            PRINTS(":");
            value = p_calue->pd;
            PRINT_DEC(&value,4);
            PRINTS(" ");
            value = p_calue->slope;
            PRINT_HEX(&value,4);
            value = p_calue->offset;
            PRINT_HEX(&value,4);
            PRINTS(" \r\n");
        } else{
            PRINTS("oops");
        }
    }
    if( !match_command(argv[0], "erase") ){
        battery_init();
        ADC_CalMgr_Remove(p_calue);
        ADC_CalMgr_EraseAll();
    }
    if( !match_command(argv[0], "cal") ){
        uint32_t slope;

        result = battery_get_initial_cached(0);

        PRINTS(" +");

        //  236 (2.000) 356 (3.000) 380 (3.200) 476(4.000)
        value = (argc > 1)?nrf_atoi(argv[1]):3000;

        PRINTS("calibrated for: ");
        PRINT_DEC(&value, 4);
        PRINTS("\r\n");

        slope = battery_get_slope_cached();
        if (result > value){
            slope = slope - (((result - value) * slope) / value);
        }
        if (result < value){
            slope = slope + (((value - result) * slope) / value);
        }
 
        PRINT_DEC(&value,4);
        PRINTS(":");
        PRINT_DEC(&slope,4);
        PRINTS(" \r\n");

        if( slope >=  6800 && slope <= 7500){
            p_calue = &calue;
            p_calue->slope = slope;
            ADC_CalMgr_Add(p_calue);
            ADC_CalMgr_Commit();
            battery_set_slope_cached(slope);
            PRINTS("Pass");
        }else{
            PRINTS("Fail");
        }

    }
    if( !match_command(argv[0], "upd") ){
        cli_update_battery_level(1); // measure battery / issue ant packet
    }
    if( !match_command(argv[0], "advstop")){
        sd_ble_gap_adv_stop();
    }
    //dispatch message through ANT
    if(argc > 1 && !match_command(argv[0], "ant") ){
        //Create a message object from uart string
        MSG_Data_t * data = MSG_Base_AllocateStringAtomic(argv[1]);
        if(data){
            self.parent->dispatch(  (MSG_Address_t){CLI, 0}, //source address, CLI
                                    (MSG_Address_t){ANT,1},//destination address, ANT
                                    data);
            //release message object after dispatch to prevent memory leak
            MSG_Base_ReleaseDataAtomic(data);
        }
    }
}

MSG_CliUserListener_t *  Cli_User_Init(MSG_Central_t * parent, void * ctx){
    self.listener.handle_command = _handle_command;
    self.parent = parent;
    return &self.listener;
}
