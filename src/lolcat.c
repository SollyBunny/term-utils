#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lib/argparse.h"
#include "lib/rand.h"

#define COLOR_MODE_NORMAL -1
#define COLOR_MODE_BASIC 0
#define COLOR_MODE_MONOCHROME 1
static int colorMode;
static bool colorRandom;
static bool colorInvert;
static bool drawBold;

static int colorsBasic(int i) {
    static int colors[] = {
        196, 202, 208, 214, 220, 226,
        190, 154, 118, 82, 46,
        48, 49, 50, 51,
        45, 39, 33, 27, 21,
        57, 93, 129, 165, 201
    };
    static int colorSize = sizeof(colors) / (sizeof(colors[0]));
    return colors[i % colorSize];
}

static int colorsMonochrome(int i) {
    const int b = 238;
    const int w = 255;
    i = i % ((w - b) * 2);
    return b + (i <= (w - b) ? i : (w - b) * 2 - i);
}

static void printChunk(const char* s, size_t n) {
    static int x = 0;
    static int y = 0;
    int pos;
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
            default:
                if (colorRandom)
                    pos = randGet(0, 2048);
                else
                    pos = x + y;
                if (colorMode == COLOR_MODE_NORMAL) {
                    const double freq = 0.1;
                    const int r = (int)(sin(freq * pos) * 127 + 128);
                    const int g = (int)(sin(freq * pos + 2 * M_PI / 3) * 127 + 128);
                    const int b = (int)(sin(freq * pos + 4 * M_PI / 3) * 127 + 128);
                    printf("\x1b[%c8;2;%d;%d;%dm%c", colorInvert ? '4' : '3', r, g, b, c);
                } else {
                    const int color = colorMode == COLOR_MODE_MONOCHROME ? colorsMonochrome(pos) : colorsBasic(pos);
                    printf("\x1b[%c8;5;%dm%c", colorInvert ? '4' : '3', color, c);
                }
                ++x;
                break;
        }
    }
}

static void printReset() {
    fputs("\x1b[0m", stdout);
    fflush(stdout);
}

static void doFile(const char* path) {
    static char buffer[4096];

    FILE* file = NULL;

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

int main(int argc, char** argv) {
    bool isHelp = false;
    bool isColorModeBasic = false;
    bool isColorModeMonochrome = false;
    colorRandom = false;
    colorInvert = false;
    drawBold = false;
    ArgparseParam params[] = {
        ARGPARSE_HELP(&isHelp),
        ARGPARSE_FLAG("-b", "--basic", &isColorModeBasic),
        ARGPARSE_FLAG("-m", "--monochrome", &isColorModeMonochrome),
        ARGPARSE_FLAG("-r", "--random", &colorRandom),
        ARGPARSE_FLAG("-i", "--invert", &colorInvert),
        ARGPARSE_FLAG(NULL, "--bold", &drawBold),
        ARGPARSE_END
    };

    argparse(argc, argv, params, false);

    if (isColorModeMonochrome + isColorModeBasic > 1) {
        fprintf(stderr, "Only one of -m or -b can be specified\n");
        return 1;
    } else if (isColorModeMonochrome) {
        colorMode = COLOR_MODE_MONOCHROME;
    } else if (isColorModeBasic) {
        colorMode = COLOR_MODE_BASIC;
    } else {
        colorMode = COLOR_MODE_NORMAL;
    }

    if (colorRandom)
        randInit();

    if (drawBold)
        fputs("\x1b[1m", stdout);

    if (isHelp) {
        const char help[] = "Usage: lolcat ... [FILE]\n"
            "ConCATenate files or stdin to stdout with colors!\n"
            "If no file is given or - is given, read stdin\n"
            "\n"
            "\t-h --help             Show this message\n"
            "\t-b --basic            Use only basic 256 colors\n"
            "\t-m --monochrome       Use only black and white\n"
            "\t-r --random           Use colors in a random order\n"
            "\t-i --invert           Invert fg and bg\n"
            "\t--bold                Use bold\n"
            "\n";
        printChunk(help, sizeof(help));
        printReset();
        return 0;
    }

    bool hasArg = false;
    for (int i = 1; i < argc; ++i) {
        bool isParam = false;

        for (ArgparseParam *param = params; param->type != ARGPARSE_TYPE_END; ++param) {
            if (
                (param->shortName && strcmp(argv[i], param->shortName) == 0) ||
                (param->longName && strcmp(argv[i], param->longName) == 0)
            ) {
                isParam = true;
                break;
            }
        }

        if (isParam)
            continue;

        hasArg = true;
        doFile(argv[i]);
    }

    if (!hasArg)
        doFile(NULL);

    printReset();
    return 0;
}
