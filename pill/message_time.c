#include "message_time.h"
#include "util.h"

static struct{
    MSG_Base_t base;
    bool initialized;
    struct rtc_time_t time;
    MSG_Central_t * central;
}self;

static char * name = "TIME";

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

static MSG_Status
_send(MSG_ModuleType src, MSG_Data_t * data){
    PRINTS("GOT TIME EVENT");
    if(data){
        MSG_Base_AcquireDataAtomic(data);
        MSG_TimeCommand_t * tmp = data->buf;
        switch(tmp->cmd){
            default:
            case PING:
                break;
            case SYNC:
                self.time = tmp->param.time;
                break;
            case SET_1_S_PERIODIC:
                break;
        }

        MSG_Base_ReleaseDataAtomic(data);
    }
    return SUCCESS;
}

MSG_Base_t * MSG_Time_Init(const MSG_Central_t * central){
    if(!self.initialized){
        self.base.init = _init;
        self.base.destroy = _destroy;
        self.base.flush = _flush;
        self.base.send = _send;
        self.base.type = TIME;
        self.base.typestr = name;
        self.central = central;
        self.initialized = 1;
    }
    return &self.base;
}
const struct rtc_time_t * MSG_Time_GetTime(void){
    return &self.time;
}
