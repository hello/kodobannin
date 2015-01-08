#include "cli_user.h"
#include "util.h"
#include "ant_driver.h"
#include <nrf_soc.h>
#include "message_led.h"
#include "app.h"
#include <ble.h>

static struct{
    //parent is the reference to the dispatcher 
    MSG_Central_t * parent;
    MSG_CliUserListener_t listener;
}self;

static void
_handle_command(int argc, char * argv[]){
    int i;
    if(argc > 1 && !match_command(argv[0], "echo")) ){
        PRINTS(argv[1]);
    }
    if( !match_command(argv[0], "led") ){
        test_led();
    }
    if( !match_command(argv[0], "advstop")){
        sd_ble_gap_adv_stop();
    }
    if( !match_command(argv[0], "imuoff") ){
        self.parent->unloadmod(MSG_IMU_GetBase());
    }
    if( !match_command(argv[0], "imuon") ){
        self.parent->loadmod(MSG_IMU_GetBase());
    }
    //dispatch message through ANT
    if(argc > 1 && !match_command("ant") ){
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
    if(argc > 2 && _strncmp(argv[0], "testant", strlen("testant")) == 0){
        PRINTS("ant radio test: \r\n");
        uint8_t freq = (uint8_t)nrf_atoi(argv[1]);
        uint8_t power = (uint8_t)nrf_atoi(argv[2]);
        PRINTS("freq = 0x");
        PRINT_HEX(&freq,1);
        PRINTS("power = 0x");
        PRINT_HEX(&power,1);
        PRINTS("\r\n");
        hlo_ant_cw_test(freq,power);
    }
}

MSG_CliUserListener_t *  Cli_User_Init(MSG_Central_t * parent, void * ctx){
    self.listener.handle_command = _handle_command;
    self.parent = parent;
    return &self.listener;
}
