#pragma once

#include <stdatomic.h>
#include <stdbool.h>

extern atomic_bool termInterrupted;

static const int TERM_CHAR_UP = 256 + 'A';
static const int TERM_CHAR_DOWN = 256 + 'B';
static const int TERM_CHAR_RIGHT = 256 + 'C';
static const int TERM_CHAR_LEFT = 256 + 'D';

void termInit();
void termDestroy();
char termRead();
int termReadFancy(bool lower);
void termReadSetBlocking(bool blocking);

typedef struct {
    int w;
    int h;
} TermSize;

/**
 * Always returns first time
 * Copy this object before next call
 */
TermSize* termSizeUpdate();

// Waits for the terminal to be moved into the FG
bool termWaitForUnpause();
