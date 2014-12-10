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
    if(argc > 1 && _strncmp(argv[0], "spi", strlen("spi")) == 0){
        MSG_Data_t * data = MSG_Base_AllocateStringAtomic(argv[1]);
        if(data){
            self.parent->dispatch(  (MSG_Address_t){CLI, 0}, //source address, CLI
                                    (MSG_Address_t){SSPI,1},//destination address, ANT
                                    data);
            //release message object after dispatch to prevent memory leak
            MSG_Base_ReleaseDataAtomic(data);
        }
    }
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
    if(_strncmp(argv[0], "resume", strlen("resume")) == 0){
        PRINTS("Resume radio = ");
        int32_t ret = hlo_ant_resume_radio();
        PRINT_HEX(&ret, 4);
        PRINTS("\r\n");
    }
    if(_strncmp(argv[0], "pause", strlen("pause")) == 0){
        PRINTS("Pause radio = ");
        int32_t ret = hlo_ant_pause_radio();
        PRINT_HEX(&ret, 4);
        PRINTS("\r\n");
    }
    if(_strncmp(argv[0], "boot", strlen("boot")) == 0){
        //prints out id
        MSG_Data_t * id = MSG_Base_AllocateDataAtomic(6);
        if(id){
            uint8_t id_copy[6] = {0};
            memcpy(id_copy, NRF_FICR->DEVICEADDR, 6);
            id->buf[0] = id_copy[5];
            id->buf[1] = id_copy[4];
            id->buf[2] = id_copy[3];
            id->buf[3] = id_copy[2];
            id->buf[4] = id_copy[1];
            id->buf[5] = id_copy[0];
            //transform for factory test
            self.parent->dispatch(  (MSG_Address_t){CLI, 0}, //source address, CLI
                                    (MSG_Address_t){UART,1},//destination address, ANT
                                    id);
            //release message object after dispatch to prevent memory leak
            MSG_Base_ReleaseDataAtomic(id);
        }
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
    if(_strncmp(argv[0], "slip", strlen("slip")) == 0){
        MSG_Data_t * data = MSG_Base_AllocateStringAtomic(argv[1]);
        if(data){
            self.parent->dispatch(  (MSG_Address_t){CLI, 0}, //source address, CLI
                                    (MSG_Address_t){UART,2},//destination address, ANT
                                    data);
            //release message object after dispatch to prevent memory leak
            MSG_Base_ReleaseDataAtomic(data);
        }
    }
    if(_strncmp(argv[0], "dfu", strlen("dfu")) == 0){
        REBOOT_TO_DFU();
    }
#ifdef FACTORY_APP
    if(argc > 0 && _strncmp(argv[0], "dtm", strlen("dtm")) == 0){
        sd_power_gpregret_set((uint32_t)GPREGRET_APP_BOOT_TO_DTM);
        sd_nvic_SystemReset();
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
#endif
}

MSG_CliUserListener_t *  Cli_User_Init(MSG_Central_t * parent, void * ctx){
    self.listener.handle_command = _handle_command;
    self.parent = parent;
    return &self.listener;
}
