#pragma once

#include <stdbool.h>

void termInit(void (*handleSig)(int));
void termDestroy();
char termReadNonblock();

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
