#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "lib/argparse.h"
#include "lib/rand.h"
#include "lib/term.h"

typedef ArgparseColor Color;

static float density;
static int lineLength;
static int lineGap;
static bool english;
static Color lineColor;
static int colorVariation;

static const char *ENGLISH_CHARS[] = {
	"a", "b", "c",	"d", "e", "f", "g", "h", "i",  "j", "k", "l", "m", "n",
	"o", "p", "q",	"r", "s", "t", "u", "v", "w",  "x", "y", "z", "A", "B",
	"C", "D", "E",	"F", "G", "H", "I", "J", "K",  "L", "M", "N", "O", "P",
	"Q", "R", "S",	"T", "U", "V", "W", "X", "Y",  "Z", "0", "1", "2", "3",
	"4", "5", "6",	"7", "8", "9", " ", "!", "\"", "#", "$", "%", "&", "'",
	"(", ")", "*",	"+", ",", "-", ".", "/", ":",  ";", "<", "=", ">", "?",
	"@", "[", "\\", "]", "^", "_", "`", "{", "|",  "}", "~"};
static const char *JAPANESE_CHARS[] = {
	"ア", "イ", "ウ", "エ", "オ", "カ", "キ", "ク", "ケ", "コ", "サ", "シ",
	"ス", "セ", "ソ", "タ", "チ", "ツ", "テ", "ト", "ナ", "ニ", "ヌ", "ネ",
	"ノ", "ハ", "ヒ", "フ", "ヘ", "ホ", "マ", "ミ", "ム", "メ", "モ", "ヤ",
	"ユ", "ヨ", "ラ", "リ", "ル", "レ", "ロ", "ワ", "ヲ", "ン", "あ", "い",
	"う", "え", "お", "か", "き", "く", "け", "こ", "さ", "し", "す", "せ",
	"そ", "た", "ち", "つ", "て", "と", "な", "に", "ぬ", "ね", "の", "は",
	"ひ", "ふ", "へ", "ほ", "ま", "み", "む", "め", "も", "や", "ゆ", "よ",
	"ら", "り", "る", "れ", "ろ", "わ", "を", "ん"};

typedef struct {
	bool active; // Should this col be simulated
	int x, y, l; // x, y pos of head and l length of tail
	int nonce;
} Line;

static TermSize termSize = {};
static size_t linesAlloced = 0;
static Line *lines = NULL;
// How long it's been since the last in each col
static int *lineGaps = NULL;

static unsigned char randomizeColorComponent(unsigned char c) {
	int delta = randGet(-colorVariation / 2, colorVariation / 2);
	int result = c + delta;

	if (result < 0)
		result = 0;
	if (result > 255)
		result = 255;

	return (unsigned char)result;
}

static const char *randomChar(void) {
	if (english)
		return ENGLISH_CHARS[randGet(
			0, sizeof(ENGLISH_CHARS) / sizeof(*ENGLISH_CHARS) - 1)];
	else
		return JAPANESE_CHARS[randGet(
			0, sizeof(JAPANESE_CHARS) / sizeof(*JAPANESE_CHARS) - 1)];
}

// Step a line
static void lineSim(Line *line) {
	line->y += 1;
	if (line->y > termSize.h + line->l)
		line->active = false;
}

static void lineDrawDot(const Line *line, int y) {
	printf("\x1b[%u;%uH", y + 1, line->x + 1);
	switch (lineColor.mode) {
	case ARGPARSE_COLOR_MODE_NORMAL: {
		printf("\x1b[38;2;%u;%u;%um", randomizeColorComponent(lineColor.r),
			   randomizeColorComponent(lineColor.g),
			   randomizeColorComponent(lineColor.b));
		break;
	}
	case ARGPARSE_COLOR_MODE_MONOCHROME: {
		const unsigned val = randomizeColorComponent(line->nonce % 255);
		printf("\x1b[38;2;%u;%u;%um", val, val, val);
		break;
	}
	case ARGPARSE_COLOR_MODE_RAINBOW: {
		const float pos = (float)line->x / (float)termSize.w * M_PI * 2;
		printf("\x1b[38;2;%u;%u;%um",
			   randomizeColorComponent(sinf(pos) * 127 + 128),
			   randomizeColorComponent(sinf(pos + 2 * M_PI / 3) * 127 + 128),
			   randomizeColorComponent(sinf(pos + 4 * M_PI / 3) * 127 + 128));
		break;
	}
	case ARGPARSE_COLOR_MODE_RANDOM: {
		printf("\x1b[38;2;%u;%u;%um",
			   randomizeColorComponent(line->nonce % 255),
			   randomizeColorComponent(line->nonce / 255 % 255),
			   randomizeColorComponent(line->nonce / 255 / 255 % 255));
		break;
	}
	case ARGPARSE_COLOR_MODE_BASIC: {
		printf("\x1b[3%cm", line->nonce % ('6' - '1' + 1) + '1');
		break;
	}
	}
	fputs(randomChar(), stdout);
}

// Draw the whole line
static void lineDrawWhole(const Line *line) {
	const int yStart = line->y >= termSize.h ? termSize.h - 1 : line->y;
	const int yEnd = line->y >= line->l ? line->y - line->l : 0;

	for (int y = yStart; y >= yEnd; --y)
		lineDrawDot(line, y);
}

// Draw just the head and clear the tail
static void lineDraw(const Line *line) {
	// Draw the character if inside the terminal.
	if (line->y < termSize.h && line->x < termSize.w)
		lineDrawDot(line, line->y);

	if (line->y >= line->l && line->y - line->l < termSize.h) {
		printf("\x1b[%u;%uH"
			   " ",
			   line->y - line->l + 1, line->x + 1);
	}
}

static size_t lineNew(int x) {
	// Find new place
	size_t i = 0;
	for (i = 0; i < linesAlloced && lines[i].active; ++i)
		;
	if (i >= linesAlloced) // No space
		return -1;
	// Make new line
	const int l = lineLength + randGet(0, 4);
	lines[i] = (Line){true, x, 0, l, randGet(0, 255 * 255 * 255)};
	return i;
}

// Fill col with lines
static void fillCol(int x) {
	int y = randGet(0, lineGap + lineLength);
	lineGaps[x] = y - lineLength;
	while (y < termSize.h + lineLength) {
		const size_t index = lineNew(x);
		if (index == (size_t)-1)
			break;
		Line *line = &lines[index];
		line->y = y;
		lineDrawWhole(line);
		y += line->l + lineGap + randGet(0, 4);
	}
}

static void redrawAll() {
	for (size_t i = 0; i < linesAlloced; ++i) {
		Line *line = &lines[i];
		if (line->active)
			lineDrawWhole(line);
	}
	fflush(stdout);
}

static void handleResize() {
	const TermSize termSizeOld = termSize;
	{
		const TermSize *termSizeNew = termSizeUpdate();
		if (termSizeNew == NULL)
			return;
		termSize = *termSizeNew;
	}
	// Handle line size changing
	const size_t linesAllocedOld = linesAlloced;
	linesAlloced =
		(size_t)((float)termSize.w * (float)termSize.h * density * 1.5f);
	lines = realloc(lines, sizeof(*lines) * linesAlloced);
	if (!lines)
		abort();
	for (size_t i = linesAllocedOld; i < linesAlloced; ++i)
		lines[i] = (Line){}; // Clear new lines
	lineGaps = realloc(lineGaps, sizeof(*lineGaps) * termSize.w);
	// Destroy old cols
	const size_t maxPossibleLineIndex =
		linesAlloced > linesAllocedOld ? linesAllocedOld : linesAlloced;
	if (termSize.w < termSizeOld.w) {
		for (size_t i = 0; i < maxPossibleLineIndex; ++i) {
			Line *line = &lines[i];
			if (line->active && line->x >= termSize.w)
				line->active = false;
		}
	}
	// Fill new cols
	for (int x = termSizeOld.w; x < termSize.w; ++x) {
		lineGaps[x] = 0;
		fillCol(x);
	}
	// Destroy old rows
	if (termSize.h < termSizeOld.h) {
		for (size_t i = 0; i < maxPossibleLineIndex; ++i) {
			Line *line = &lines[i];
			if (line->active && line->y > termSize.h + line->l)
				line->active = false;
		}
	}
	// Fill new rows
	if (termSize.h > termSizeOld.h) {
		for (size_t i = 0; i < maxPossibleLineIndex; ++i) {
			Line *line = &lines[i];
			if (line->active)
				continue;
			if (line->y >= termSizeOld.h)
				lineDrawWhole(line);
		}
	}
	// Flush
	fflush(stdout);
}

static void destroy() {
	termDestroy();
	free(lines);
	free(lineGaps);
}

int main(int argc, char **argv) {
	bool isHelp = false;
	float frameDelay = 0.05;
	lineLength = 15;
	density = 0.5;
	lineColor = (Color){100, 255, 100, ARGPARSE_COLOR_MODE_NORMAL};
	colorVariation = 50;
	english = false;
	ArgparseParam params[] = {
		ARGPARSE_HELP(&isHelp),
		ARGPARSE_FLOAT("-d", "--delay", &frameDelay, 0, 1),
		// ARGPARSE_INT(NULL, "--linelength", &lineLength, 2, 100),
		// ARGPARSE_FLOAT(NULL, "--density", &density, 0.1, 0.9),
		ARGPARSE_COLOR("-c", "--color", &lineColor),
		// ARGPARSE_INT(NULL, "--colorvariation", &colorVariation, 0, 255),
		ARGPARSE_FLAG(NULL, "--english", &english), ARGPARSE_END};

	argparse(argc, argv, params, NULL);
	if (isHelp) {
		printf("Usage: %s ...\n"
			   "Show a fullscreen matrix style scrolling effect\n"
			   "\n"
			   "\t-h --help             Show this message\n"
			   "\t-c --color <c>        RRGGBB, RGB, rrr;ggg;bbb\n"
			   "\t                      monochrome, rainbow, random, basic\n"
			   // "\t--colorvariation <i>  How much each color component can
			   // vary (default 30)\n"
			   // "\t--linelength <i>      How long on average each line should
			   // be (default: 15)\n"
			   "\t-d --delay <f>        Delay in seconds between frames "
			   "(default: 0.05)\n"
			   // "\t--density <f>         Ratio of space that should be filled
			   // with lines (default 0.5)\n"
			   "\t--english             Use english characters instead of "
			   "japanese\n"
			   "\n",
			   argv[0]);
		return 0;
	}

	// density = lineLength / (lineLength + lineGap)
	lineGap = ((float)lineLength / density) - lineLength;
	if (lineGap < 1)
		lineGap = 1;

	randInit();
	termInit();
	handleResize();
	for (int x = 0; x < termSize.w; ++x)
		fillCol(x);
	fflush(stdout);

	bool paused = false;
	while (true) {
		if (!paused)
			sleepStable(frameDelay);
		if (termInterrupted)
			break;
		if (termWaitForUnpause())
			redrawAll();
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
		} else if (c == 'j') {
			english = !english;
			redrawAll();
		}
		handleResize();
		if (paused)
			continue;
		for (int x = 0; x < termSize.w; ++x) {
			if (lineGaps[x] > lineGap && randGet(0, 1) == 1) {
				const size_t index = lineNew(x);
				if (index == (size_t)-1)
					break;
				const Line *line = &lines[index];
				lineGaps[x] = -line->l;
			}
		}
		for (int x = 0; x < termSize.w; ++x)
			lineGaps[x] += 1;
		for (size_t i = 0; i < linesAlloced; ++i) {
			if (!lines[i].active)
				continue;
			lineDraw(&lines[i]);
			lineSim(&lines[i]);
		}
		fflush(stdout);
	}
	destroy();
	return 0;
}
