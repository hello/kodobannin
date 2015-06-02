#ifndef HLO_QUEUE_H
#define HLO_QUEUE_H

struct hlo_queue_t;
typedef struct hlo_queue_t hlo_queue_t;

struct hlo_queue_t * hlo_queue_init(size_t size_in_power_of_two);

uint32_t hlo_queue_write(struct hlo_queue_t * queue, unsigned char * src, size_t size);
uint32_t hlo_queue_read(struct hlo_queue_t * queue, unsigned char * dst, size_t size);
size_t hlo_queue_filled_size(struct hlo_queue_t * queue);
size_t hlo_queue_empty_size(struct hlo_queue_t * queue);



#endif
