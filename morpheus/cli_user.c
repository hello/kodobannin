#include "cli_user.h"
#include "util.h"

static struct{
    MSG_CliUserListener_t listener;
}self;

static void
_handle_command(int argc, char * argv[], void * ctx){
    /*
     * stub code
     */
    int i;
    PRINTS("Args:\r\n");
    for(i = 0; i < argc; i++){
        PRINTS(argv[i]);
        PRINTS("\r\n");
    }
}

MSG_CliUserListener_t *  Cli_User_Init(void * ctx){
    self.listener.handle_command = _handle_command;
    self.listener.user_ctx = ctx;
    return &self.listener;
}
