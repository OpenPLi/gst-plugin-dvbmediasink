#include <gst/gst.h>

#include "common.h"

void queue_push(queue_entry_t **queue_base, GstBuffer *buffer, size_t start, size_t end)
{
	queue_entry_t *entry = g_malloc(sizeof(queue_entry_t));
	queue_entry_t *last = *queue_base;
	gst_buffer_ref(buffer);
	entry->buffer = buffer;
	entry->start = start;
	entry->end = end;
	if (!last)
	{
		*queue_base = entry;
	}
	else
	{
		while (last->next) last = last->next;
		last->next = entry;
	}
	entry->next = NULL;
}

void queue_pop(queue_entry_t **queue_base)
{
	queue_entry_t *base = *queue_base;
	*queue_base = base->next;
	gst_buffer_unref(base->buffer);
	g_free(base);
}

int queue_front(queue_entry_t **queue_base, GstBuffer **buffer, size_t *start, size_t *end)
{
	if (!*queue_base)
	{
		*buffer = NULL;
		*start = 0;
		*end = 0;
		return -1;
	}
	else
	{
		queue_entry_t *entry = *queue_base;
		*buffer = entry->buffer;
		*start = entry->start;
		*end = entry->end;
		return 0;
	}
}

void pes_set_pts(long long timestamp, unsigned char *pes_header)
{
	unsigned long long pts = timestamp * 9LL / 100000; /* convert ns to 90kHz */
	pes_header[9] =  0x21 | ((pts >> 29) & 0xE);
	pes_header[10] = pts >> 22;
	pes_header[11] = 0x01 | ((pts >> 14) & 0xFE);
	pes_header[12] = pts >> 7;
	pes_header[13] = 0x01 | ((pts << 1) & 0xFE);
}

void pes_set_payload_size(size_t size, unsigned char *pes_header)
{
	if (size > 0xffff) size = 0;
	pes_header[4] = size >> 8;
	pes_header[5] = size & 0xFF;
}

