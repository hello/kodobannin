#include "heap.h"
#include "app_fifo.h"
#include "util.h"

struct hlo_queue_t{
    app_fifo_t fifo;
    size_t filled; 
    size_t capacity;
};

struct hlo_queue_t * hlo_queue_init(size_t size_in_power_of_two){
    uint32_t code;
    struct hlo_queue_t * ret = pvPortMalloc(sizeof(*ret));
    APP_ASSERT(ret);
    uint8_t * buf = pvPortMalloc(size_in_power_of_two);
    APP_ASSERT(buf);
    code = app_fifo_init(&ret->fifo, buf, size_in_power_of_two);
    if(code != NRF_SUCCESS){
        vPortFree(ret);
        vPortFree(buf);
        return NULL;
    }
    ret->capacity = size_in_power_of_two;
    ret->filled = 0;
    return ret;
}

uint32_t hlo_queue_write(struct hlo_queue_t * queue, unsigned char * src, size_t size){
    APP_ASSERT(queue);
    if(queue->filled + size <= queue->capacity){
        int i;
        for(i = 0; i < size; i++){
            APP_OK(app_fifo_put(&queue->fifo, src[i]));
        }
        queue->filled += size;
        return NRF_SUCCESS;
    }
    return NRF_ERROR_INVALID_LENGTH;
}
uint32_t hlo_queue_read(struct hlo_queue_t * queue, unsigned char * dst, size_t size){
    APP_ASSERT(queue);
    if(queue->filled >= size){
        int i;
        for(i = 0; i < size; i++){
            APP_OK(app_fifo_get(&queue->fifo, dst+i));
        }
        queue->filled -= size;
        return NRF_SUCCESS;
    }
    return NRF_ERROR_INVALID_LENGTH;
}

size_t hlo_queue_filled_size(struct hlo_queue_t * queue){
    APP_ASSERT(queue);
    return queue->filled;
}
size_t hlo_queue_empty_size(struct hlo_queue_t * queue){
    APP_ASSERT(queue);
    return queue->capacity - queue->filled;
}
