#ifndef _common_h
#define _common_h

typedef struct queue_entry
{
	GstBuffer *buffer;
	struct queue_entry *next;
	size_t start;
	size_t end;
} queue_entry_t;

void queue_push(queue_entry_t **queue_base, GstBuffer *buffer, size_t start, size_t end);
void queue_pop(queue_entry_t **queue_base);
int queue_front(queue_entry_t **queue_base, GstBuffer **buffer, size_t *start, size_t *end);

void pes_set_pts(long long timestamp, unsigned char *pes_header);
void pes_set_payload_size(size_t size, unsigned char *pes_header);

#endif