#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lib/ansii.h"
#include "lib/argparse.h"
#include "lib/rand.h"
#include "lib/term.h"

static ArgparseColor lavaColor;

static struct {
	int w, h;
	uint8_t *data;
} screen = {0, 0, NULL};

typedef struct {
	float x, y;
	float dir;
} Ball;
static size_t ballsCount = 0;
static Ball *balls = NULL;

static const float ballRadius = 10.0f;
static const float merginess = 2.0f;

static inline int min(int a, int b) { return a < b ? a : b; }

static inline int modNeg(int a, int n) { return a - a / n * n; }

static float fmodfNeg(float a, float n) { return a - floorf(a / n) * n; }

static float shortestPathF(float a, float b, float m) {
	float d = a - b;
	return fmodfNeg(d + m / 2, m) - m / 2;
}

static void destroy() {
	termDestroy();
	free(screen.data);
	free(balls);
}

static void ballSim(Ball *ball) {
	float da = (float)randGet(-1024, 1024) / 1024; // Linear between -1 and 1
	da = da * da * da;			   // Sigmoidish between -1 and 1
	ball->dir += da * (M_PI / 10); // Between -18 and 18 deg
	ball->dir = fmodfNeg(ball->dir, M_PI * 2);
	const float a =
		ball->dir +
		(float)randGet(-1024, 1024) / 1024 *
			(M_PI / 30); // 45 deg of random to represent the angle accurately
	ball->x += sinf(a) / 5;
	ball->y += cosf(a) / 5;
	ball->x = fmodfNeg(ball->x, screen.w);
	ball->y = fmodfNeg(ball->y, screen.h);
}

static inline Ball ballNew() {
	return (Ball){
		.x = randGet(0, screen.w - 1),
		.y = randGet(0, screen.h - 1),
		.dir = (float)randGet(0, 2048) / 2048 * (M_PI * 2),
	};
}

static inline AnsiiColor getColor(int x, int y, uint8_t n) {
	const float v = (float)n / 255;
	switch (lavaColor.mode) {
	case ARGPARSE_COLOR_MODE_NORMAL:
		return ansiiRGB(lavaColor.r * fmaxf(v, 0.5),
						lavaColor.g * fmaxf(v, 0.5),
						lavaColor.b * fmaxf(v, 0.5));
	case ARGPARSE_COLOR_MODE_MONOCHROME: {
		n = min((int)n + 100, 255);
		return ansiiRGB(n, n, n);
	}
	case ARGPARSE_COLOR_MODE_RAINBOW: {
		const float pos = v * M_PI * 2;
		return ansiiRGB(sinf(pos) * 127 + 128,
						sinf(pos + 2 * M_PI / 3) * 127 + 128,
						sinf(pos + 4 * M_PI / 3) * 127 + 128);
	}
	case ARGPARSE_COLOR_MODE_RANDOM: {
		const float pos = (float)clock() / 1e6;
		return ansiiRGB(sinf(v + pos) * 127 + 128,
						sinf(v + pos + (float)x / 10) * 127 + 128,
						sinf(v + pos + (float)y / 10) * 127 + 128);
	}
	case ARGPARSE_COLOR_MODE_BASIC:
		if (n > 254)
			return ansii256(15);
		if (n > 253)
			return ansii256(14);
		if (n > 200)
			return ansii256(10);
		return ansii256(11);
	}
	abort();
}

static void handleResize(TermSize termSize) {
	screen.w = (termSize.w + 1) / 2;
	screen.h = termSize.h;
	screen.data = realloc(screen.data, screen.w * screen.h * sizeof(uint8_t));
	memset(screen.data, 0, screen.w * screen.h * sizeof(uint8_t));
	// Balls
	ballsCount = min(screen.w, screen.h) / 8;
	balls = realloc(balls, ballsCount * sizeof(*balls));
	for (size_t i = 0; i < ballsCount; ++i)
		balls[i] = ballNew();
}

int main(int argc, char **argv) {
	bool isHelp = false;
	float frameDelay = 0.05;
	lavaColor = (ArgparseColor){0, 0, 0, ARGPARSE_COLOR_MODE_RANDOM};
	ArgparseParam params[] = {
		ARGPARSE_HELP(&isHelp),
		ARGPARSE_FLOAT("-d", "--delay", &frameDelay, 0, 1),
		ARGPARSE_COLOR("-c", "--color", &lavaColor), ARGPARSE_END};

	argparse(argc, argv, params, NULL);
	if (isHelp) {
		printf("Usage: %s ...\n"
			   "Pipe screensaver\n"
			   "\n"
			   "\t-h --help             Show this message\n"
			   "\t-c --color <c>        RRGGBB, RGB, rrr;ggg;bbb\n"
			   "\t                      monochrome, rainbow, random, basic\n"
			   "\t-d --delay <f>        Delay in seconds between frames "
			   "(default: 0.05)\n"
			   "\n",
			   argv[0]);
		return 0;
	}

	randInit();

	termSetSettings(
		(TermSettings){.lower = true, .readBlocking = false, .readTime = 0});
	termInit();

	handleResize(termGetSize());

	// A ball's field is (1 - (r / reach)^2)^2 inside `reach`. `iso` is the
	// field value exactly at `ballRadius`, so an isolated ball ends there.
	const float reach = ballRadius * merginess;
	const float invReachSq = 1 / (reach * reach);
	const float edge = 1 - 1 / (merginess * merginess);
	const float iso = edge * edge;
	const float altitudeScale = 1 / (1 - iso);

	bool paused = false;
	while (true) {
		if (!paused)
			sleepStable(frameDelay);

		TermEvent event = termGetEvent();
		if (event.type == TERM_EVENT_INTERUPT) {
			break;
		} else if (event.type == TERM_EVENT_RESIZED) {
			ansiiReset();
			ansiiClearScreen();
			handleResize(event.data.resized);
		} else if (event.type == TERM_EVENT_PAUSED) {
			memset(screen.data, 0, screen.w * screen.h * sizeof(*screen.data));
		} else if (event.type == TERM_EVENT_KEY) {
			int key = event.data.key;
			if (key == ' ') {
				paused = !paused;
				sleepStableReset();
				termSetSettings((TermSettings){
					.lower = true, .readBlocking = paused, .readTime = 0});
			}
		}

		if (paused)
			continue;

		for (size_t i = 0; i < ballsCount; ++i)
			ballSim(&balls[i]);

		for (int y = 0; y < screen.h; ++y) {
			for (int x = 0; x < screen.w; ++x) {
				// Sum the balls' fields. The kernels have compact support, so
				// the blob ends at a hard iso-contour, but the fields overlap
				// far before the visible surfaces touch.
				float field = 0;
				for (size_t i = 0; i < ballsCount; ++i) {
					const Ball *ball = &balls[i];
					const float dx = shortestPathF(ball->x, x, screen.w);
					const float dy = shortestPathF(ball->y, y, screen.h);
					const float q = (dx * dx + dy * dy) * invReachSq;
					if (q >= 1)
						continue;
					const float k = 1 - q;
					field += k * k;
				}

				int n = 0;
				if (field >= iso) {
					// Height of the metaball surface above the screen.
					// Brighter = higher, with no directional light.
					float alt = sqrtf((field - iso) * altitudeScale);
					if (alt > 1)
						alt = 1;
					n = fminf(255, alt * 255);
				}
				uint8_t *storedN = &screen.data[y * screen.w + x];
				if (*storedN != n) {
					*storedN = n;
					ansiiMove(x * 2, y);
					if (n == 0)
						ansiiResetBg();
					else
						ansiiBg(getColor(x, y, n));
					fputs("  ", stdout);
				}
			}
		}
		fflush(stdout);
	}
	destroy();
	return 0;
}
