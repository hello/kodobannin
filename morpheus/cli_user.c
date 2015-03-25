#include "cli_user.h"
#include "util.h"
#include <nrf_soc.h>
#include <string.h>
#include "ant_driver.h"
#include "app.h"

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
    if(argc > 1 && !match_command(argv[0], "spi") ){
        MSG_Data_t * data = MSG_Base_AllocateStringAtomic(argv[1]);
        if(data){
            self.parent->dispatch(  (MSG_Address_t){CLI, 0}, //source address, CLI
                                    (MSG_Address_t){SSPI,1},//destination address, ANT
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
