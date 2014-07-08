#include <stddef.h>
#include <app_util.h>
#include <nrf_soc.h>
#include "message_base.h"
#include "util.h"
static struct{
    MSG_Data_t pool[MSG_BASE_SHARED_POOL_SIZE]; 
    MSG_Data_t * cached;
}self;

MSG_Data_t * MSG_Base_AllocateDataAtomic(uint32_t size){
    MSG_Data_t * ret = NULL;
    if( size > MSG_BASE_DATA_BUFFER_SIZE ){
        return NULL;
    }
    CRITICAL_REGION_ENTER();
    for(int i = 0; i < MSG_BASE_SHARED_POOL_SIZE; i++){
        if(self.pool[i].ref == 0){
            ret = &self.pool[i];
            ret->len = size;
            ret->ref = 1;
            PRINTS("+");
            break;
        }
    }
    CRITICAL_REGION_EXIT();
    return ret;
}
//TODO
//this method is unsafe, switch to strncpy later
MSG_Data_t * MSG_Base_AllocateStringAtomic(const char * str){
    if(!str) return NULL;
    int n = strlen(str);
    n = MIN(MSG_BASE_DATA_BUFFER_SIZE, n);
    MSG_Data_t * ret = MSG_Base_AllocateDataAtomic(n);
    if(ret){
        memcpy(ret->buf, str, n);
        return ret;
    }else{
        return NULL;
    }
}

MSG_Status MSG_Base_AcquireDataAtomic(MSG_Data_t * d){
    if(d){
        CRITICAL_REGION_ENTER();
        d->ref++;
        PRINTS("+");
        CRITICAL_REGION_EXIT();
        return SUCCESS;
    }
    return FAIL;
}
MSG_Status MSG_Base_ReleaseDataAtomic(MSG_Data_t * d){
    if(d){
        CRITICAL_REGION_ENTER();
        if(d->ref){
            d->ref--;
            PRINTS("-");
            if(d->ref==0){
                PRINTS("~");
            }
        }
        CRITICAL_REGION_EXIT();
    }
    return FAIL;
}

