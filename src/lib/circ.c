#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "circ.h"

CircBuffer circCreate(size_t cap, size_t elementSize) {
	if (cap == 0 || elementSize == 0 || cap > SIZE_MAX / elementSize)
		abort();

	CircBuffer buf = {0, cap, 0, elementSize, malloc(cap * elementSize)};

	if (!buf.data)
		abort();

	return buf;
}

void circFree(CircBuffer *buf) {
	if (!buf)
		return;

	free(buf->data);
	buf->data = NULL;

	buf->len = 0;
	buf->cap = 0;
}

void *circPop(CircBuffer *buf) {
	if (!buf || !buf->data || buf->len == 0)
		return NULL;

	// Remove the last logical element
	size_t index = (buf->head + buf->len - 1) % buf->cap;

	void *element = (char *)buf->data + index * buf->elementSize;

	buf->len--;

	if (buf->len == 0)
		buf->head = 0;

	return element;
}

void *circAdd(CircBuffer *buf) {
	if (!buf || !buf->data)
		return NULL;

	if (buf->len == buf->cap) {
		// Double capacity
		const size_t newCap = buf->cap * 2;

		if (newCap < buf->cap || newCap > SIZE_MAX / buf->elementSize)
			abort();

		void *newData = malloc(newCap * buf->elementSize);

		if (!newData)
			abort();

		// Copy existing elements, preserving logical order
		for (size_t i = 0; i < buf->len; i++) {
			size_t index = (buf->head + i) % buf->cap;

			memcpy((char *)newData + (i + 1) * buf->elementSize,
				   (char *)buf->data + index * buf->elementSize,
				   buf->elementSize);
		}

		free(buf->data);

		buf->data = newData;
		buf->cap = newCap;
		buf->head = 0;
	} else {
		// Move head backwards to make room at the front
		buf->head = (buf->head + buf->cap - 1) % buf->cap;
	}

	buf->len++;

	// Return the new front element
	return (char *)buf->data + buf->head * buf->elementSize;
}
