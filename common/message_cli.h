#pragma once
#include "message_base.h"

#define MSG_CLI_MAX_COMMANDS 5
#define MSG_CLI_MAX_NAME_LEN 8

typedef MSG_Status ( * msg_cli_handler) (const MSG_Cli_Params param, char * out_opt_str);

MSG_Status MSG_Cli_Initialize(void);
MSG_Status MSG_Cli_Register_Command(const char * name, msg_cli_cmd cmd);
MSG_Status MSG_Cli_Exec(const MSG_Data_t * d);

