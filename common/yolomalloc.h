#pragma once

/**
 * Yolomalloc.h
 * mallocs from a chunk of memory, never frees
 * used for ephermeral applications like the bootloader
 * not thread save
 */

#include <stdint.h>
typedef struct{
    uint32_t offset;
    uint32_t size;
    uint8_t block[0];
}yolo_ctx_t;

yolo_ctx_t * yolo_init(void * buffer, size_t buffer_size);
void * yolo_malloc(size_t size);
void * yolo_calloc(size_t num, size_t size);
void yolo_free(void * ptr);



