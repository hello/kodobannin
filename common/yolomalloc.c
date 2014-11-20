#include "yolomalloc.h"
#include <string.h>

yolo_ctx_t * yolo_init(void * buffer, size_t buffer_size){
    if(buffer){
        if(buffer_size > sizeof(yolo_ctx_t)){
            yolo_ctx_t * ret = (yolo_ctx_t *)buffer;
            ret->offset = 0;
            ret->size = buffer_size - sizeof(yolo_ctx_t);
            return ret;
        }
    }
    return NULL;
}
void * yolo_malloc(yolo_ctx_t * ctx, size_t size){
    if(ctx->offset + size <= ctx->size){
        void * ret = (void*)(&ctx->block[ctx->offset]);
        ctx->offset += size;
        return ret;
    }else{
        return NULL;
    }
}
void * yolo_calloc(yolo_ctx_t * ctx, size_t num, size_t size){
    void * ret = yolo_malloc(num * size);
    if(ret){
        memset(ret, 0, num * size);
    }
    return ret;
}

void yolo_free(void * ptr){
    //?
}
