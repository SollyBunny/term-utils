#include <math.h>
#include <stdio.h>
#include <string.h>

#include "lib/argparse.h"
#include "lib/rand.h"

typedef ArgparseColor Color;

static Color color;
static bool colorInvert;
static bool drawBold;

static unsigned char clampedAddReflect(unsigned char a, unsigned char b) {
	const int res = (int)a + (int)b;
	if (res < 0)
		return -res;
	if (res > 255)
		return 255 - (res - 255);
	return res;
}

static int colorsBasic(int i) {
	static int colors[] = {196, 202, 208, 214, 220, 226, 190, 154, 118,
						   82,	46,	 48,  49,  50,	51,	 45,  39,  33,
						   27,	21,	 57,  93,  129, 165, 201};
	static int colorSize = sizeof(colors) / (sizeof(colors[0]));
	return colors[i % colorSize];
}

static int colorsMonochrome(int i) {
	const int b = 238;
	const int w = 255;
	i = i % ((w - b) * 2);
	return b + (i <= (w - b) ? i : (w - b) * 2 - i);
}

static void printChunk(const char *s, size_t n) {
	static int x = 0;
	static int y = 0;
	for (size_t i = 0; i < n; ++i) {
		const char c = s[i];
		switch (c) {
		case ' ':
			putchar(' ');
			++x;
			break;
		case '\t':
			fputs("    ", stdout);
			x += 4;
			break;
		case '\n':
			if (drawBold)
				fputs("\x1b[0m\n\x1b[1m", stdout);
			else
				fputs("\x1b[0m\n", stdout);
			x = 0;
			++y;
			break;
		default: {
			const int pos = x + y;
			switch (color.mode) {
			case ARGPARSE_COLOR_MODE_NORMAL:
			case ARGPARSE_COLOR_MODE_RAINBOW:
			case ARGPARSE_COLOR_MODE_RANDOM: {
				const float freq = 0.1;
				unsigned char r, g, b;
				if (color.mode == ARGPARSE_COLOR_MODE_NORMAL) {
					r = clampedAddReflect(color.r,
										  (sinf(pos * freq) * 127 + 128) / 3);
					g = clampedAddReflect(
						color.g,
						(sinf(pos * freq + 2 * M_PI / 3) * 127 + 128) / 3);
					b = clampedAddReflect(
						color.b,
						(sinf(pos * freq + 4 * M_PI / 3) * 127 + 128) / 3);
				} else if (color.mode == ARGPARSE_COLOR_MODE_RAINBOW) {
					r = sinf(pos * freq) * 127 + 128;
					g = sinf(pos * freq + 2 * M_PI / 3) * 127 + 128;
					b = sinf(pos * freq + 4 * M_PI / 3) * 127 + 128;
				} else if (color.mode == ARGPARSE_COLOR_MODE_RANDOM) {
					r = randGet(0, 255);
					g = randGet(0, 255);
					b = randGet(0, 255);
				}
				printf("\x1b[%c8;2;%d;%d;%dm%c", colorInvert ? '4' : '3', r, g,
					   b, c);
				break;
			}
			case ARGPARSE_COLOR_MODE_MONOCHROME:
			case ARGPARSE_COLOR_MODE_BASIC: {
				printf("\x1b[%c8;5;%dm%c", colorInvert ? '4' : '3',
					   color.mode == ARGPARSE_COLOR_MODE_MONOCHROME
						   ? colorsMonochrome(pos)
						   : colorsBasic(pos),
					   c);
				break;
			}
			}
			++x;
			break;
		}
		}
	}
}

static void printReset() {
	fputs("\x1b[0m", stdout);
	fflush(stdout);
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
	color = (Color){0, 0, 0, ARGPARSE_COLOR_MODE_RAINBOW};
	drawBold = false;
	ArgparseParam params[] = {
		ARGPARSE_HELP(&isHelp), ARGPARSE_COLOR("-c", "--color", &color),
		ARGPARSE_FLAG("-i", "--invert", &colorInvert),
		ARGPARSE_FLAG("-b", "--bold", &drawBold), ARGPARSE_END};

	bool consumed[argc];
	memset(consumed, false, sizeof(consumed));
	argparse(argc, argv, params, consumed);

	if (color.mode == ARGPARSE_COLOR_MODE_RANDOM)
		randInit();

	if (drawBold)
		fputs("\x1b[1m", stdout);

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
		printReset();
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

	printReset();
	return 0;
}
