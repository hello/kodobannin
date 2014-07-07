#include <stddef.h>
#include "message_cli.h"

typedef struct{
    char name[MSG_CLI_MAX_NAME_LEN];
    msg_cli_handler handler;
}MSG_Cmd_t;

struct{
    MSG_Cmd_t cli_cmds[MSG_CLI_MAX_COMMANDS];
}self;

MSG_Status MSG_Cli_Initialize(void){
    for(int i = 0; i < MSG_CLI_MAX_COMMANDS; i++){
        self.cli_cmds[i].handler = NULL;
        self.cli_cmds[i].name[0] = '\0';
    }
}
MSG_Status MSG_Cli_Register_Command(const char * name, msg_cli_cmd cmd){
    MSG_Cmd_t * tmp;
    for(int i = 0; i < MSG_CLI_MAX_COMMANDS; i++){
        tmp = &self.cli_cmds[i];
        if(!tmp -> handler){
            tmp->handler = cmd;
            strncpy(tmp->name, name, MSG_CLI_MAX_NAME_LEN);
            return SUCCESS;
        }
    }
    return OOM;
}
MSG_Status MSG_Cli_Exec(const MSG_Data_t * d){
    message_cli

}
