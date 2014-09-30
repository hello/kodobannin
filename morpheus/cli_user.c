#include "cli_user.h"
#include "util.h"

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
_handle_command(int argc, char * argv[], void * ctx){
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
}

MSG_CliUserListener_t *  Cli_User_Init(MSG_Central_t * parent, void * ctx){
    self.listener.handle_command = _handle_command;
    self.listener.user_ctx = ctx;
    self.parent = parent;
    return &self.listener;
}
