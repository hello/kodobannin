#include "cli_user.h"
#include "util.h"
#include <nrf_soc.h>
#include <string.h>
#include "app_timer.h"
#include "ant_driver.h"
#include "app.h"
#include "hble.h"
#include "heap.h"

#include <nrf_gpio.h>
#include <nrf_delay.h>

static struct{
    //parent is the reference to the dispatcher 
    MSG_Central_t * parent;
    MSG_CliUserListener_t listener;
}self;

#include "message_ant.h"
#include "ant_devices.h"
static void
_handle_command(int argc, char * argv[]){
    if(argc > 1 && !match_command(argv[0], "echo")){
        PRINTS(argv[1]);
    }
    if(argc > 1 && !match_command(argv[0], "spi") ){
        MSG_Data_t * data = NULL;
        uint8_t addr = 0;
        if(argc > 2){
            addr = nrf_atoi(argv[1]);
            data = MSG_Base_AllocateStringAtomic(argv[2]);
        }else{
            data = MSG_Base_AllocateStringAtomic(argv[1]);
        }
        if(data){
            self.parent->dispatch(  (MSG_Address_t){CLI, 0}, //source address, CLI
                    (MSG_Address_t){SSPI,addr},//destination address, ANT
                    data);
            //release message object after dispatch to prevent memory leak
            MSG_Base_ReleaseDataAtomic(data);
        }
    }
    if(argc > 1 && !match_command(argv[0], "ant")){
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
    if( !match_command(argv[0], "resume") ){
        PRINTS("Resume radio = ");
        int32_t ret = hlo_ant_resume_radio();
        PRINT_HEX(&ret, 4);
        PRINTS("\r\n");
    }
    if( !match_command(argv[0], "pause") ){
        PRINTS("Pause radio = ");
        int32_t ret = hlo_ant_pause_radio();
        PRINT_HEX(&ret, 4);
        PRINTS("\r\n");
    }
    if( !match_command(argv[0], "time") ){
        static uint32_t time = 0;
        static uint32_t rtc = 0;
        uint32_t current_time = 0;
        app_timer_cnt_get(&current_time);
        
        if( argc > 1 ) {
            time = nrf_atoi(argv[1]);
            PRINTF("Setting time %d\n", time);
            rtc = current_time;
        } else if(time) {
            uint32_t time_diff;
            app_timer_cnt_diff_compute(current_time, rtc, &time_diff);
            time_diff /= APP_TIMER_TICKS( 1000, APP_TIMER_PRESCALER );
            PRINTF("\n_ set-time %d\n", time + time_diff);
        }
    }
    if( !match_command(argv[0], "prox") ){
        hlo_ant_device_t device = (hlo_ant_device_t){
            .device_type = HLO_ANT_DEVICE_TYPE_PILL,
            .rssi = -60,
        };

        unsigned char buf[20] = {0};
        MSG_ANT_PillData_t * pill_data = (MSG_ANT_PillData_t*)buf;
        pill_data->type = ANT_PILL_PROX_ENCRYPTED;
        pill_data->version = 2;
        pill_data->payload_len = sizeof(buf) - sizeof(*pill_data);
        pill_data->UUID = 0;

        MSG_Data_t * message = MSG_Base_AllocateObjectAtomic(pill_data, sizeof(buf));
        if(!message){
            return;
        }
        MSG_ANT_Message_t content = (MSG_ANT_Message_t){
            .device = device,
            .message = message,
        };

        MSG_Data_t * parcel = MSG_Base_AllocateObjectAtomic(&content, sizeof(content));
        if(parcel){
            self.parent->dispatch( ADDR(ANT,0), ADDR(ANT,MSG_ANT_HANDLE_MESSAGE), parcel);
            MSG_Base_ReleaseDataAtomic(parcel);
            MSG_Base_ReleaseDataAtomic(message);
        }
    }
    if( !match_command(argv[0], "bounce") ){
        PRINTS("Bouncing 3.3v rail...\r\n");
        nrf_delay_ms(500);
        
        nrf_gpio_cfg_output(0);
        nrf_gpio_pin_clear(0);
        nrf_delay_ms(100);
        nrf_gpio_pin_set(0);

    }
    if( !match_command(argv[0], "free") ){
        PRINTF("Free Memory = %d Least Memory = %d\r\n", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize() );
    }
    if( !match_command(argv[0], "boot") ){
        //force boot without midboard
        MSG_Data_t * data = MSG_Base_AllocateDataAtomic(1);
        if(data){
            self.parent->dispatch(  (MSG_Address_t){CLI, 0}, //source address, CLI
                                    (MSG_Address_t){BLE,10},//destination address, ANT
                                    data);
            //release message object after dispatch to prevent memory leak
            MSG_Base_ReleaseDataAtomic(data);
        }
    }
    if( !match_command(argv[0], "slip") && argc > 1 ){
        MSG_Data_t * data = MSG_Base_AllocateStringAtomic(argv[1]);
        if(data){
            self.parent->dispatch(  (MSG_Address_t){CLI, 0}, //source address, CLI
                                    (MSG_Address_t){UART,2},//destination address, ANT
                                    data);
            //release message object after dispatch to prevent memory leak
            MSG_Base_ReleaseDataAtomic(data);
        }
    }
#ifdef FACTORY_APP
    if( !match_command(argv[0], "dtm") ){
        sd_power_gpregret_set((uint32_t)GPREGRET_APP_BOOT_TO_DTM);
        sd_nvic_SystemReset();
    }
#endif
    if( !match_command(argv[0], "recover") ){
		REBOOT_WITH_ERROR(GPREGRET_APP_RECOVER_BONDS);
    }
}

MSG_CliUserListener_t *  Cli_User_Init(MSG_Central_t * parent, void * ctx){
    self.listener.handle_command = _handle_command;
    self.parent = parent;
    return &self.listener;
}
