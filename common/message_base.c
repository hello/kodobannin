#include <stddef.h>
#include <app_util.h>
#include <nrf_soc.h>
#include "message_base.h"
#include "util.h"

#define POOL_OBJ_SIZE(buf_size) (buf_size + sizeof(MSG_Data_t))

static struct{
    uint8_t pool[POOL_OBJ_SIZE(MSG_BASE_DATA_BUFFER_SIZE) * MSG_BASE_SHARED_POOL_SIZE];
#ifdef MSG_BASE_USE_BIG_POOL
    uint8_t bigpool[POOL_OBJ_SIZE(MSG_BASE_DATA_BUFFER_SIZE_BIG) * MSG_BASE_SHARED_POOL_SIZE_BIG];
#endif
}self;

MSG_Data_t * MSG_Base_AllocateDataAtomic(uint32_t size){
    MSG_Data_t * ret = NULL;
    uint32_t step_size = POOL_OBJ_SIZE(MSG_BASE_DATA_BUFFER_SIZE);
    uint32_t step_limit = MSG_BASE_SHARED_POOL_SIZE;
    uint8_t * p = self.pool;
    if( size > MSG_BASE_DATA_BUFFER_SIZE ){
#ifdef MSG_BASE_USE_BIG_POOL
        if( size > MSG_BASE_DATA_BUFFER_SIZE_BIG ){
            step_size = POOL_OBJ_SIZE(MSG_BASE_DATA_BUFFER_SIZE_BIG);
            step_limit = MSG_BASE_SHARED_POOL_SIZE_BIG;
            p = self.bigpool;
            goto allocate;
        }
#else
        return NULL;
#endif
    }
allocate:
    CRITICAL_REGION_ENTER();
    for(int i = 0; i < step_limit; i++){
        MSG_Data_t * tmp = (MSG_Data_t*)(&p[i*step_size]);
        if(tmp->ref == 0){
            tmp->len = size;
            tmp->ref = 1;
            ret = tmp;
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
#ifndef MSG_BASE_DATA_BUFFER_SIZE
                //here we free the buffer
#endif
            }
        }
        CRITICAL_REGION_EXIT();
    }
    return FAIL;
}
static void
_free_all(){

}
MSG_Status
MSG_Base_BufferTest(void){
    for(int i = 0; i < MSG_BASE_SHARED_POOL_SIZE; i++){
        MSG_Data_t * o = MSG_Base_AllocateDataAtomic(1); 
        MSG_Data_t * b = MSG_Base_AllocateDataAtomic(1); 
        MSG_Data_t * j = MSG_Base_AllocateDataAtomic(1); 
        MSG_Base_ReleaseDataAtomic(o);
        MSG_Base_ReleaseDataAtomic(j);
    }
    for(int i = 0; i < MSG_BASE_SHARED_POOL_SIZE_BIG; i++){
        MSG_Data_t * obj = MSG_Base_AllocateDataAtomic(MSG_BASE_SHARED_POOL_SIZE_BIG); 
        MSG_Data_t * o = MSG_Base_AllocateDataAtomic(MSG_BASE_SHARED_POOL_SIZE_BIG); 
        MSG_Data_t * b = MSG_Base_AllocateDataAtomic(MSG_BASE_SHARED_POOL_SIZE_BIG); 
        MSG_Data_t * j = MSG_Base_AllocateDataAtomic(MSG_BASE_SHARED_POOL_SIZE_BIG); 
        MSG_Base_ReleaseDataAtomic(o);
        MSG_Base_ReleaseDataAtomic(j);
    }
    MSG_Data_t * obj = MSG_Base_AllocateDataAtomic(1); 
    MSG_Data_t * objbig = MSG_Base_AllocateDataAtomic(MSG_BASE_DATA_BUFFER_SIZE_BIG); 
    if(obj || objbig){
        PRINTS("Failed Test\r\n");
        return FAIL;
    }else{
        PRINTS("All Pass\r\n");
    }
    return SUCCESS;
}

