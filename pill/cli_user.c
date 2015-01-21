#include "cli_user.h"
#include "util.h"
#include "ant_driver.h"
#include <nrf_soc.h>
#include "message_led.h"
#include "message_imu.h"
#include "app.h"
#include "battery.h"
#include <ble.h>

static struct{
    //parent is the reference to the dispatcher 
    MSG_Central_t * parent;
    MSG_CliUserListener_t listener;
}self;

static void
_handle_command(int argc, char * argv[]){
    int i;
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
#ifdef PLATFORM_HAS_VLED
    if( !match_command(argv[0], "bat") ){
        test_bat();
    }
    if( !match_command(argv[0], "red") ){
        test_red();
    }
    if( !match_command(argv[0], "grn") ){
        test_grn();
    }
    if( !match_command(argv[0], "blu") ){
        test_blu();
    }
    if( !match_command(argv[0], "rgb") ){
        test_rgb();
    }
    if( !match_command(argv[0], "led") ){
        test_led();
    }
    if( !match_command(argv[0], "dsp") ){
        led_update_battery_status();
    }
#endif
    if( !match_command(argv[0], "mon") ){
        hble_update_battery_level(0); // monitor droop
    }
    if( !match_command(argv[0], "upd") ){
        hble_update_battery_level(1); // measure battery / issue ant packet
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
