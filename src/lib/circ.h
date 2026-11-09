#pragma once

#include <stddef.h>

typedef struct {
	size_t len;
	size_t cap;
	size_t head;
	size_t elementSize;
	void *data;
} CircBuffer;

CircBuffer circCreate(size_t cap, size_t elementSize);
void circFree(CircBuffer *buf);

void *circPop(CircBuffer *buf);
void *circAdd(CircBuffer *buf);

#define CIRC_INDEX(buf, type, i)                                               \
	(((type *)(buf).data)[((i) + (buf).head) % (buf).cap])
