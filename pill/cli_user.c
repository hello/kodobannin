#include "cli_user.h"
#include "util.h"
#include "ant_driver.h"
#include <nrf_soc.h>
#include "message_led.h"
#include "message_led.h"

static struct{
    //parent is the reference to the dispatcher 
    MSG_Central_t * parent;
    MSG_CliUserListener_t listener;
}self;

static int
_strncmp(const char * s1, const char *s2, uint32_t n){
    while(n--){
        if(*s1++!=*s2++){
            return *(uint8_t*)(s1-1) - *(uint8_t*)(s2-1);
        }
    }
    return 0;
}
static void
_handle_command(int argc, char * argv[]){
    /*
     * stub code
     */
    int i;
    PRINTS("Received Args:\r\n");
    for(i = 0; i < argc; i++){
        PRINTS(argv[i]);
        PRINTS("\r\n");
    }
    //echo command
    PRINTS("Handle Command: \r\n");
    if(argc > 1 && _strncmp(argv[0], "echo", strlen("echo")) == 0){
        PRINTS(argv[1]);
    }
    if(_strncmp(argv[0], "dfu", strlen("dfu")) == 0){
        REBOOT_TO_DFU();
    }
    if(_strncmp(argv[0], "led", strlen("led")) == 0){
        test_led();
    }
    if(_strncmp(argv[0], "bat", strlen("bat")) == 0){
        test_bat();
    }
    if(_strncmp(argv[0], "rgb", strlen("rgb")) == 0){
        test_rgb();
    }
    if(_strncmp(argv[0], "dsp", strlen("dsp")) == 0){
        led_update_battery_status();
    }
    if(_strncmp(argv[0], "upd", strlen("upd")) == 0){
        hble_update_battery_level(1); // make battery capacity assessment
    }
    //dispatch message through ANT
    if(argc > 1 && _strncmp(argv[0], "ant", strlen("ant")) == 0){
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
