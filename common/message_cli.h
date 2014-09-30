#pragma once
#include "message_base.h"

#define CLI_MAX_ARGS 5

typedef struct{
    void (*handle_command)(int argc, char * argv[], void * ctx);
    void * user_ctx;
}MSG_CliUserListener_t;

MSG_Base_t * MSG_Cli_Base(const MSG_Central_t * parent, const MSG_CliUserListener_t * user);

