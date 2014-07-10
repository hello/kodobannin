#include "timedfifo.h"
#include "util.h"

static tf_unit_t buffer[TF_BUFFER_SIZE];
static uint16_t idx;
static uint64_t fifo_time;

static void
_inc_idx(void){
    PRINTS("*");
    idx = (idx + 1)%TF_BUFFER_SIZE;
    buffer[idx] = 0;
}

void TF_Initialize(uint64_t initial_time){
    fifo_time = initial_time;
    idx = 0;
}
uint16_t TF_UpdateIdx(uint64_t current_time){
    //checks current_time vs fifo_time, update idx as necessary
    uint16_t inc = (current_time - fifo_time)/TF_UNIT_TIME_MS;
    //inc += ( (current_time - fifo_time > 0)?1:0);
    for(int i = 0; i < inc; i++){
        _inc_idx();
    }
    fifo_time = current_time;
    return idx;
}
tf_unit_t * TF_GetCurrent(void){
    return &buffer[idx];
}
