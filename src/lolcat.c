#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lib/ansii.h"
#include "lib/argparse.h"
#include "lib/rand.h"

static ArgparseColor textColor;
static bool colorInvert;
static bool drawBold;

static uint8_t clampedAddReflect(uint8_t a, int b) {
	const int res = (int)a + b;
	if (res < 0)
		return -res;
	if (res > 255)
		return 255 - (res - 255);
	return res;
}

static uint8_t colorsBasic(int i) {
	static uint8_t colors[] = {196, 202, 208, 214, 220, 226, 190, 154, 118,
							   82,	46,	 48,  49,  50,	51,	 45,  39,  33,
							   27,	21,	 57,  93,  129, 165, 201};
	return colors[i % (sizeof(colors) / (sizeof(colors[0])))];
}

static uint8_t colorsMonochrome(int i) {
	const uint8_t b = 238;
	const uint8_t w = 255;
	i = i % ((w - b) * 2);
	return b + (i <= (w - b) ? i : (w - b) * 2 - i);
}

static void printChunk(const char *s, size_t n) {
	static int x = 0;
	static int y = 0;
	static bool bgColored = false;
	static bool boldApplied = false;
	for (size_t i = 0; i < n; ++i) {
		const char c = s[i];
		switch (c) {
		case ' ':
			if (bgColored) {
				ansiiResetBg();
				bgColored = false;
			}
			putchar(' ');
			++x;
			break;
		case '\t':
			if (bgColored) {
				ansiiResetBg();
				bgColored = false;
			}
			fputs("    ", stdout);
			x += 4;
			break;
		case '\n':
			if (bgColored) {
				ansiiResetBg();
				bgColored = false;
			}
			putchar('\n');
			x = 0;
			++y;
			break;
		default: {
			const int pos = x + y;
			AnsiiColor color;
			switch (textColor.mode) {
			case ARGPARSE_COLOR_MODE_NORMAL:
			case ARGPARSE_COLOR_MODE_RAINBOW:
			case ARGPARSE_COLOR_MODE_RANDOM: {
				const float freq = 0.1;
				uint8_t r, g, b;
				if (textColor.mode == ARGPARSE_COLOR_MODE_NORMAL) {
					r = clampedAddReflect(textColor.r,
										  (sinf(pos * freq) * 127 + 128) / 3);
					g = clampedAddReflect(
						textColor.g,
						(sinf(pos * freq + 2 * M_PI / 3) * 127 + 128) / 3);
					b = clampedAddReflect(
						textColor.b,
						(sinf(pos * freq + 4 * M_PI / 3) * 127 + 128) / 3);
				} else if (textColor.mode == ARGPARSE_COLOR_MODE_RAINBOW) {
					r = sinf(pos * freq) * 127 + 128;
					g = sinf(pos * freq + 2 * M_PI / 3) * 127 + 128;
					b = sinf(pos * freq + 4 * M_PI / 3) * 127 + 128;
				} else if (textColor.mode == ARGPARSE_COLOR_MODE_RANDOM) {
					r = randGet(0, 255);
					g = randGet(0, 255);
					b = randGet(0, 255);
				}
				color = ansiiRGB(r, g, b);
				break;
			}
			case ARGPARSE_COLOR_MODE_MONOCHROME:
				color = ansii256(colorsMonochrome(pos));
				break;
			case ARGPARSE_COLOR_MODE_BASIC:
				color = ansii256(colorsBasic(pos));
				break;
			}
			if (!boldApplied && drawBold) {
				boldApplied = true;
				ansiiBold();
			}
			ansiiColor(color, colorInvert);
			putchar(c);
			if (colorInvert)
				bgColored = true;
			++x;
			break;
		}
		}
	}
}

static void doFile(const char *path) {
	static char buffer[4096];

	FILE *file = NULL;

	if (!path || strcmp(path, "-") == 0) {
		file = stdin;
	} else {
		file = fopen(path, "r");
		if (file == NULL) {
			perror(path);
			return;
		}
	}

	size_t n = 0;
	while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0)
		printChunk(buffer, n);

	if (file != stdin)
		fclose(file);
}

int main(int argc, char **argv) {
	bool isHelp = false;
	textColor = (ArgparseColor){0, 0, 0, ARGPARSE_COLOR_MODE_RAINBOW};
	drawBold = false;
	ArgparseParam params[] = {
		ARGPARSE_HELP(&isHelp), ARGPARSE_COLOR("-c", "--color", &textColor),
		ARGPARSE_FLAG("-i", "--invert", &colorInvert),
		ARGPARSE_FLAG("-b", "--bold", &drawBold), ARGPARSE_END};

	bool consumed[argc];
	memset(consumed, false, sizeof(consumed));
	argparse(argc, argv, params, consumed);

	if (textColor.mode == ARGPARSE_COLOR_MODE_RANDOM)
		randInit();

	if (isHelp) {
		const char help[] =
			"Usage: lolcat ... [FILE]\n"
			"ConCATenate files or stdin to stdout with colors!\n"
			"If no file is given or - is given, read stdin\n"
			"\n"
			"\t-h --help             Show this message\n"
			"\t-c --color <c>        RRGGBB, RGB, rrr;ggg;bbb\n"
			"\t                      monochrome, rainbow, random, basic\n"
			"\t-i --invert           Invert fg and bg\n"
			"\t-b --bold             Use bold\n"
			"\n";
		printChunk(help, sizeof(help));
		ansiiReset();
		fflush(stdout);
		return 0;
	}

	bool hasArg = false;
	for (int i = 1; i < argc; ++i) {
		if (consumed[i])
			continue;
		hasArg = true;
		doFile(argv[i]);
	}

	if (!hasArg)
		doFile(NULL);

	ansiiReset();
	fflush(stdout);
	return 0;
}
