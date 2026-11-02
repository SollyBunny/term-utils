#pragma once

#include <stdatomic.h>
#include <stdbool.h>

extern atomic_bool termInterrupted;

void termInit();
void termDestroy();
char termRead();
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
