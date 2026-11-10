#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "lib/argparse.h"
#include "lib/rand.h"
#include "lib/term.h"

typedef ArgparseColor Color;

static Color pipeColor;
static int colorVariation;
static const char **pipeChars;

static const char *PIPE_CHARS[][12] = {
	// https://github.com/pipeseroni/pipes.sh#contents
	[0] = {"┃", "┏", "┓", "┛", "━", "┓", "┗", "┃", "┛", "┗", "┏", "━"},
	[1] = {"│", "╭", "╮", "╯", "─", "╮", "╰", "│", "╯", "╰", "╭", "─"},
	[2] = {"│", "┌", "┐", "┘", "─", "┐", "└", "│", "┘", "└", "┌", "─"}, // Viewable on tty
	[3] = {"║", "╔", "╗", "╝", "═", "╗", "╚", "║", "╝", "╚", "╔", "═"},
	[4] = {"|", "+", "+", "+", "-", "+", "+", "|", "+", "+", "+", "-"},
	[5] = {"|", "/", "\\", "/", "-", "\\", "\\", "|", "/", "\\", "/", "-"},
	[6] = {".", ".", ".", ".", ".", ".", ".", ".", ".", ".", ".", "."},
	[7] = {".", "o", "o", "o", ".", "o", "o", ".", "o", "o", "o", "."},
	[8] = {"-", "\\", "/", "\\", "|", "/", "/", "-", "\\", "/", "\\", "|"},
	[9] = {"╿", "┍", "┑", "┚", "╼", "┒", "┕", "╽", "┙", "┖", "┎", "╾"}};

typedef struct {
	int x, y;
} Pos;

typedef struct {
	Pos pos;
	unsigned char dir;
	int nonce;
	unsigned char charIndex;
} Pipe;

static TermSize termSize = {};
static size_t pipesCount = 0;
static Pipe *pipes = NULL;

static void destroy() {
	termDestroy();
	free(pipes);
}

static void pipeSim(Pipe *pipe) {
	if (pipe->dir == 0)
		pipe->pos.y -= 1;
	else if (pipe->dir == 1)
		pipe->pos.x += 1;
	else if (pipe->dir == 2)
		pipe->pos.y += 1;
	else if (pipe->dir == 3)
		pipe->pos.x -= 1;
	if (pipe->pos.x < 0 || pipe->pos.x >= termSize.w || pipe->pos.y < 0 ||
		pipe->pos.y >= termSize.h) {
		pipe->pos.x = (pipe->pos.x % termSize.w + termSize.w) % termSize.w;
		pipe->pos.y = (pipe->pos.y % termSize.h + termSize.h) % termSize.h;
		pipe->nonce = randGet(0, 255 * 255 * 255);
	}
	pipe->charIndex = pipe->dir * 3;
	if (randGet(0, 10) == 0) {
		if (randGet(0, 1) == 0) {
			// CW
			pipe->dir = (pipe->dir + 1) % 4;
			pipe->charIndex += 1;
		} else {
			// ACW
			pipe->dir = pipe->dir == 0 ? 3 : pipe->dir - 1;
			pipe->charIndex += 2;
		}
	}
	const unsigned char charIndexMap[] = {0, 1, 2, 4, 5, 3, 7, 8, 6, 11, 9, 10};
	pipe->charIndex = charIndexMap[pipe->charIndex];
}

static void pipeDraw(const Pipe *pipe) {
	printf("\x1b[%d;%dH", pipe->pos.y + 1, pipe->pos.x + 1);
	switch (pipeColor.mode) {
	case ARGPARSE_COLOR_MODE_NORMAL:
		printf("\x1b[38;2;%u;%u;%um", pipeColor.r, pipeColor.g, pipeColor.b);
		break;
	case ARGPARSE_COLOR_MODE_MONOCHROME:
		printf("\x1b[38;5;%dm", pipe->nonce % (255 - 232 + 1) + 232);
		break;
	case ARGPARSE_COLOR_MODE_RAINBOW:
		printf(
			"\x1b[38;2;%u;%u;%um",
			(unsigned int)(sinf((float)pipe->nonce) * 127 + 128),
			(unsigned int)(sinf((float)pipe->nonce + 2 * M_PI / 3) * 127 + 128),
			(unsigned int)(sinf((float)pipe->nonce + 4 * M_PI / 3) * 127 +
						   128));
		break;
	case ARGPARSE_COLOR_MODE_RANDOM:
		printf("\x1b[38;2;%u;%u;%um", pipe->nonce % 255,
			   pipe->nonce / 255 % 255, pipe->nonce / 255 / 255 % 255);
		break;
	case ARGPARSE_COLOR_MODE_BASIC:
		printf("\x1b[3%cm", pipe->nonce % ('6' - '1' + 1) + '1');
		break;
	}
	fputs(pipeChars[pipe->charIndex], stdout);
}

static inline Pipe pipeNew() {
	return (Pipe){
		.pos = {randGet(0, termSize.w - 1), randGet(0, termSize.h - 1)},
		.dir = randGet(0, 3),
		.nonce = randGet(0, 255 * 255 * 255),
		.charIndex = 0,
	};
}

static void handleResize() {
	const TermSize *termSizeNew = termSizeUpdate();
	if (!termSizeNew)
		return;
	termSize = *termSizeNew;

	pipesCount = (size_t)termSize.w * (size_t)termSize.h / 100;
	pipes = realloc(pipes, pipesCount * sizeof(pipes[0]));
	for (size_t i = 0; i < pipesCount; ++i)
		pipes[i] = pipeNew();
}

int main(int argc, char **argv) {
	bool isHelp = false;
	float frameDelay = 0.05;
	int pipeSet = 2;
	pipeColor = (Color){0, 0, 0, ARGPARSE_COLOR_MODE_BASIC};
	colorVariation = 50;
	ArgparseParam params[] = {
		ARGPARSE_HELP(&isHelp), ARGPARSE_INT("-t", "--set", &pipeSet, 0, 9),
		ARGPARSE_FLOAT("-d", "--delay", &frameDelay, 0, 1),
		ARGPARSE_COLOR("-c", "--color", &pipeColor), ARGPARSE_END};

	argparse(argc, argv, params, NULL);
	if (isHelp) {
		printf("Usage: %s ...\n"
			   "Pipe screensaver\n"
			   "\n"
			   "\t-h --help             Show this message\n"
			   "\t-t --set <0-9>        Which pipe set to use\n"
			   "\t-c --color <c>        RRGGBB, RGB, rrr;ggg;bbb\n"
			   "\t                      monochrome, rainbow, random, basic\n"
			   "\t-d --delay <f>        Delay in seconds between frames "
			   "(default: 0.05)\n"
			   "\n",
			   argv[0]);
		return 0;
	}

	pipeChars = PIPE_CHARS[pipeSet];

	randInit();
	termInit();

	bool paused = false;
	while (true) {
		if (!paused)
			sleepStable(frameDelay);
		if (termInterrupted)
			break;
		termWaitForUnpause();
		const char c = termReadFancy(true);
		if (c == ' ') {
			if (paused) {
				paused = false;
				termReadSetBlocking(false);
				sleepStableReset();
			} else {
				paused = true;
				termReadSetBlocking(true);
			}
		}
		handleResize();
		if (paused)
			continue;
		for (size_t i = 0; i < pipesCount; ++i) {
			pipeSim(&pipes[i]);
			pipeDraw(&pipes[i]);
		}
		fflush(stdout);
	}
	destroy();
	return 0;
}
