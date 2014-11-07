#include <stddef.h>
#include <string.h>
#include "util.h"
#include "message_cli.h"
/**
 * much code copy pasted from
 * https://code.google.com/p/embox/source/browse/trunk/embox/src/lib/shell/tokenizer.c?r=6132
 * TODO: add quote supports
 **/
static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    MSG_CliUserListener_t user;
}self;
static char * name = "CLI";

static MSG_Status
_init(void){
    return SUCCESS;
}

static MSG_Status
_destroy(void){
    return SUCCESS;
}

static MSG_Status
_flush(void){
    return SUCCESS;
}
static bool
_isspace(char b){
    return b == ' ';
}
static char *
_next_token(char ** str){
    char * ret;
    char *ptr = *str;

    while( _isspace(*ptr)){
        ++ptr;
    }
    if(! *ptr){
        return NULL;
    }
    ret = (char *)ptr;

    while(*ptr && !_isspace(*ptr)){
        ++ptr;
    }
    *str = ptr;
    return ret;
}

static int
_tokenize(char * string, char **argv){
    int argc = 0;
    char * token;
    while(*string != '\0' && argc < CLI_MAX_ARGS){
        argv[argc++] = (token = _next_token(&string));
        if(*string){
            *string++ = '\0';
        }
    }
    return argc;
}

static MSG_Status
_handle_raw_command(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    char * argv[CLI_MAX_ARGS] = {0};
    int argc = _tokenize(data->buf,argv);
    /*
     *PRINTS("args: \r\n");
     *for(i = 0; i < argc; i++){
     *    PRINTS(argv[i]);
     *    PRINTS("\r\n");
     *}
     */
    if(self.user.handle_command){
        self.user.handle_command(argc, argv);
    }
    return SUCCESS;

}


MSG_Base_t * MSG_Cli_Base(const MSG_Central_t * parent, const MSG_CliUserListener_t * user){
    self.base.init = _init;
    self.base.destroy = _destroy;
    self.base.flush = _flush;
    self.base.send = _handle_raw_command;
    self.base.type = CLI;
    self.base.typestr = name;
    self.parent = parent;
    self.user = *user;
    return &self.base;
}
