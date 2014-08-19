#include <stdbool.h>
#include <stdint.h>
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
    typedef struct {
          uint8_t * begin, *top, *bottom, *end;
    } circular_buffer;

    void circular_buffer_init( circular_buffer * csb, void * memory, uint32_t size );
    bool circular_buffer_queue( circular_buffer * csb, void * memory, uint32_t size );
    bool circular_buffer_deque( circular_buffer * csb, void * memory, uint32_t * size );

#ifdef __cplusplus
}
#endif
