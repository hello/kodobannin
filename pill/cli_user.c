#include "cli_user.h"
#include "util.h"
#include "led_booster_timer.h"

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
    if(argc > 0 && _strncmp(argv[0], "lon", strlen("lon")) == 0){
        led_booster_power_on();
    }
    if(argc > 0 && _strncmp(argv[0], "loff", strlen("loff")) == 0){
        led_booster_power_off();
    }
}

MSG_CliUserListener_t *  Cli_User_Init(MSG_Central_t * parent, void * ctx){
    self.listener.handle_command = _handle_command;
    self.parent = parent;
    return &self.listener;
}
