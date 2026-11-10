#pragma once

#include <stdatomic.h>
#include <stdbool.h>

extern atomic_bool termInterrupted;

static const int TERM_CHAR_UP = 256 + 'A';
static const int TERM_CHAR_DOWN = 256 + 'B';
static const int TERM_CHAR_RIGHT = 256 + 'C';
static const int TERM_CHAR_LEFT = 256 + 'D';

typedef struct {
	bool lower;
	bool readBlocking;
	float readTime;
} TermSettings;

typedef struct {
	int w;
	int h;
} TermSize;

enum TermEventType {
	TERM_EVENT_NONE = 0,
	TERM_EVENT_INTERUPT,
	TERM_EVENT_RESIZED,
	TERM_EVENT_PAUSED,
	TERM_EVENT_KEY,
};

typedef struct {
	enum TermEventType type;
	union {
		TermSize resized;
		int key;
	} data;
} TermEvent;

void termInit();
void termDestroy();

void termSetSettings(TermSettings settings);

TermSize termGetSize();
TermEvent termGetEvent();

TermEvent termEvent();

// Not in own files due to linkage with interruption and pausing
void sleepStableReset();
void sleepStable(float seconds);
