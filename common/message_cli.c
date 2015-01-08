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
    const MSG_Central_t * parent;
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
static int
_strncmp(const char * s1, const char *s2, uint32_t n){
    while(n--){
        if(*s1++!=*s2++){
            return *(uint8_t*)(s1-1) - *(uint8_t*)(s2-1);
        }
    }
    return 0;
}
int match_command(const char * argv, const char * command){
    int command_length = strlen(command);
    if(command_length != strlen(argv)){
        return -1;
    }else{
        return _strncmp(argv, command, command_length);
    }
}

#include <nrf_soc.h>
#include "util.h"
static int
_handle_default_commands(int argc, char * argv[]){
    if(argc > 0){
        //reboots to dfu mode
        if(!match_command(argv[0], "dfu")){
            REBOOT_TO_DFU();
        }

        if(!match_command(argv[0], "ver")){
            MSG_Data_t * msg = MSG_Base_AllocateStringAtomic(BLE_MODEL_NUM);
            if(msg){
                self.parent->dispatch(  (MSG_Address_t){CLI, 0}, //source address, CLI
                        (MSG_Address_t){UART,MSG_UART_STRING},//destination address, UART STRING
                        msg);
                MSG_Base_ReleaseDataAtomic(msg);
            }
        }

        if(!match_command(argv[0], "id")){
            uint64_t id = GET_UUID_64();
            MSG_Data_t * msg = MSG_Base_AllocateDataAtomic(sizeof(id));
            if(msg){
                memcpy(msg->buf, &id, sizeof(id));
                self.parent->dispatch(  (MSG_Address_t){CLI, 0},
                        (MSG_Address_t){UART,MSG_UART_HEX},
                        msg);
                MSG_Base_ReleaseDataAtomic(msg);
            }

        }

        if(!match_command(argv[0], "rst")){
            REBOOT();
        }

        if(!match_command(argv[0], "mac")){
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
                        (MSG_Address_t){UART,MSG_UART_HEX},//destination address, ANT
                        id);
                //release message object after dispatch to prevent memory leak
                MSG_Base_ReleaseDataAtomic(id);
            }
        }

        if(!match_command(argv[0], "antid")){
            uint16_t device_number = GET_UUID_16();
            MSG_Data_t * id = MSG_Base_AllocateDataAtomic(sizeof(device_number));
            if(id){
                memcpy(id->buf, &device_number, sizeof(device_number));
                self.parent->dispatch(  (MSG_Address_t){CLI, 0}, //source address, CLI
                        (MSG_Address_t){UART,MSG_UART_HEX},//destination address, ANT
                        id);
                //release message object after dispatch to prevent memory leak
                MSG_Base_ReleaseDataAtomic(id);
            }
        }
    }
    return 0;
}
static MSG_Status
_handle_raw_command(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    char * argv[CLI_MAX_ARGS] = {0};
    int argc = _tokenize(data->buf,argv);

    _handle_default_commands(argc, argv);

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
