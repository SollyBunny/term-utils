#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "rand.h"

static uint64_t randState = 0;
static uint64_t randInc = 1442695040888963407ULL;

static uint32_t randNext(void) {
    uint64_t oldState = randState;
    randState = oldState * 6364136223846793005ULL + randInc;

    uint32_t xorshifted = (uint32_t)(((oldState >> 18u) ^ oldState) >> 27u);
    uint32_t rot = (uint32_t)(oldState >> 59u);

    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

void randInit(void) {
    randState = time(NULL);
    randNext();
    randState += 0x853c49e6748fea9bULL;
    randNext();
}

// Returns number from min to max inclusive roughly evenly
int randGet(int min, int max) {
    if (min > max)
        abort();

    uint64_t range = (uint64_t)((int64_t)max - min) + 1;

    // Rejection sampling eliminates modulo bias.
    uint32_t threshold = (uint32_t)(-range) % range;

    uint32_t r;
    do {
        r = randNext();
    } while (r < threshold);

    return (int)((int64_t)min + r % range);
}
