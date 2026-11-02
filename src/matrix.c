#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib/argparse.h"
#include "lib/term.h"
#include "lib/rand.h"

typedef ArgparseColor Color;

static float density;
static int lineLength;
static int lineGap;
static bool english;
static Color lineColor;
static int colorVariation;

#define COLOR_MODE_NORMAL -1
#define COLOR_MODE_RAINBOW 0
#define COLOR_MODE_MONOCHROME 1
#define COLOR_MODE_BASIC 2
static int colorMode;

static const char* englishChars[] = {
    "a", "b", "c", "d", "e", "f", "g", "h",
    "i", "j", "k", "l", "m", "n", "o", "p",
    "q", "r", "s", "t", "u", "v", "w", "x",
    "y", "z",
    "A", "B", "C", "D", "E", "F", "G", "H",
    "I", "J", "K", "L", "M", "N", "O", "P",
    "Q", "R", "S", "T", "U", "V", "W", "X",
    "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    " ", "!", "\"", "#", "$", "%", "&", "'",
    "(", ")", "*", "+", ",", "-", ".", "/",
    ":", ";", "<", "=", ">", "?", "@",
    "[", "\\", "]", "^", "_", "`",
    "{", "|", "}", "~"
};
static const char* japaneseChars[] = {
    "ア", "イ", "ウ", "エ", "オ",
    "カ", "キ", "ク", "ケ", "コ",
    "サ", "シ", "ス", "セ", "ソ",
    "タ", "チ", "ツ", "テ", "ト",
    "ナ", "ニ", "ヌ", "ネ", "ノ",
    "ハ", "ヒ", "フ", "ヘ", "ホ",
    "マ", "ミ", "ム", "メ", "モ",
    "ヤ", "ユ", "ヨ",
    "ラ", "リ", "ル", "レ", "ロ",
    "ワ", "ヲ", "ン",
    "あ", "い", "う", "え", "お",
    "か", "き", "く", "け", "こ",
    "さ", "し", "す", "せ", "そ",
    "た", "ち", "つ", "て", "と",
    "な", "に", "ぬ", "ね", "の",
    "は", "ひ", "ふ", "へ", "ほ",
    "ま", "み", "む", "め", "も",
    "や", "ゆ", "よ",
    "ら", "り", "る", "れ", "ろ",
    "わ", "を", "ん"
};

typedef struct {
    bool active; // Should this col be simulated
    int x, y, l; // x, y pos of head and l length of tail
    Color color; // Avg color of head
} Line;

static TermSize termSize = {};
static size_t linesAlloced = 0;
static Line* lines = NULL;
// How long it's been since the last in each col
static int* lineGaps = NULL;

static unsigned char randomizeColorComponent(unsigned char c) {
    int delta = randGet(-colorVariation / 2, colorVariation / 2);
    int result = c + delta;

    if (result < 0) result = 0;
    if (result > 255) result = 255;

    return (unsigned char)result;
}

static Color randomizeColor(Color color) {
    if (colorMode == COLOR_MODE_MONOCHROME) {
        const unsigned char v = randomizeColorComponent(color.r);
        return (Color){v, v, v};
    }
    return (Color){
        randomizeColorComponent(color.r),
        randomizeColorComponent(color.g),
        randomizeColorComponent(color.b)
    };
}

static const char* randomChar(void) {
    if (english)
        return englishChars[randGet(0, sizeof(englishChars) / sizeof(*englishChars) - 1)];
    else
        return japaneseChars[randGet(0, sizeof(japaneseChars) / sizeof(*japaneseChars) - 1)];
}

// Step a line
static void lineSim(Line* line) {
    line->y += 1;
    if (line->y > termSize.h + line->l)
        line->active = false;
}

static void lineDrawDot(const Line* line, int x, int y) {
    if (colorMode == COLOR_MODE_BASIC) {
        int color = randGet(0, 1) > 0 ? randGet(32, 36) : randGet(92, 96);
        printf(
            "\x1b[%u;%uH"
            "\x1b[%dm"
            "%s",
            y + 1, x + 1,
            color,
            randomChar()
        );
    } else {
        const Color color = randomizeColor(line->color);
        printf(
            "\x1b[%u;%uH"
            "\x1b[38;2;%u;%u;%um"
            "%s",
            y + 1, x + 1,
            (unsigned)color.r, (unsigned)color.g, (unsigned)color.b,
            randomChar()
        );
    }
}

// Draw the whole line
static void lineDrawWhole(const Line* line) {
    const int yStart =
        line->y >= termSize.h ? termSize.h - 1 : line->y;
    const int yEnd =
        line->y >= line->l ? line->y - line->l : 0;

    for (int y = yStart; y >= yEnd; --y)
        lineDrawDot(line, line->x, y);
}

// Draw just the head and clear the tail
static void lineDraw(const Line* line) {
    // Draw the character if inside the terminal.
    if (line->y < termSize.h && line->x < termSize.w)
        lineDrawDot(line, line->x, line->y);

    if (line->y >= line->l && line->y - line->l < termSize.h) {
        printf(
            "\x1b[%u;%uH"
            " ",
            line->y - line->l + 1, line->x + 1
        );
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
    Color color;
    if (colorMode == COLOR_MODE_MONOCHROME) {
        const unsigned char v = randGet(0, 255);
        color = (Color){v, v, v};
    } else if (colorMode == COLOR_MODE_RAINBOW) {
        // const double pos = (double)randGet(0, 2048) / 2048 * M_PI * 2; // Actually random
        const double pos = (double)x / (double)termSize.w * M_PI * 2 + ((double)(randGet(0, 64) - 32) / 128);
        color = (Color){
            (int)(sin(pos) * 127 + 128),
            (int)(sin(pos + 2 * M_PI / 3) * 127 + 128),
            (int)(sin(pos + 4 * M_PI / 3) * 127 + 128)
        };
    } else {
        color = lineColor;
    }
    lines[i] = (Line){
        true, x, 0, l, color
    };
    return i;
}

// Fill col with lines
static void fillCol(int x) {
    int y = randGet(0, lineGap + lineLength);
    lineGaps[x] = y - lineLength;
    while (y < termSize.h + lineLength) {
        const size_t index = lineNew(x);
        if (index == (size_t) - 1) break;
        Line* line = &lines[index];
        line->y = y;
        lineDrawWhole(line);
        y += line->l + lineGap + randGet(0, 4);
    }
}

static void redrawAll() {
    for (size_t i = 0; i < linesAlloced; ++i) {
        Line* line = &lines[i];
        if (line->active)
            lineDrawWhole(line);
    }
    fflush(stdout);
}

static void handleResize() {
    const TermSize termSizeOld = termSize;
    {
        const TermSize* termSizeNew = termSizeUpdate();
        if (termSizeNew == NULL)
            return;
        termSize = *termSizeNew;
    }
    // Handle line size changing
    const size_t linesAllocedOld = linesAlloced;
    linesAlloced = (size_t)((float)termSize.w * (float)termSize.h * density * 1.5f);
    lines = realloc(lines, sizeof(*lines) * linesAlloced);
    if (!lines)
        abort();
    for (size_t i = linesAllocedOld; i < linesAlloced; ++i)
        lines[i] = (Line){}; // Clear new lines
    lineGaps = realloc(lineGaps, sizeof(*lineGaps) * termSize.w);
    // Destroy old cols
    const size_t maxPossibleLineIndex = linesAlloced > linesAllocedOld ? linesAllocedOld : linesAlloced;
    if (termSize.w < termSizeOld.w) {
        for (size_t i = 0; i < maxPossibleLineIndex; ++i) {
            Line* line = &lines[i];
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
            Line* line = &lines[i];
            if (line->active && line->y > termSize.h + line->l)
                line->active = false;
        }
    }
    // Fill new rows
    if (termSize.h > termSizeOld.h) {
        for (size_t i = 0; i < maxPossibleLineIndex; ++i) {
            Line* line = &lines[i];
            if (line->active)
                continue;
            if (line->y >= termSizeOld.h)
                lineDrawWhole(line);
        }
    }
    // Flush
    fflush(stdout);
}

static volatile atomic_bool interrupted = false;

static void destroy() {
    termDestroy();
    free(lines);
    free(lineGaps);
}

static void handleSig(int sig) {
    (void)sig;
    if (interrupted)
        return;
    interrupted = true;
}

int main(int argc, char** argv) {
    bool isHelp = false;
    float frameDelay = 0.05;
    lineLength = 15;
    density = 0.5;
    bool isColorModeRainbow = false;
    bool isColorModeMonochrome = false;
    bool isColorModeBasic = false;
    lineColor = (Color){100, 255, 100};
    colorVariation = 20;
    english = false;
    ArgparseParam params[] = {
        ARGPARSE_HELP(&isHelp),
        ARGPARSE_FLOAT("-d", "--delay", &frameDelay, 0, 1),
        // ARGPARSE_INT(NULL, "--linelength", &lineLength, 2, 100),
        // ARGPARSE_FLOAT(NULL, "--density", &density, 0.1, 0.9),
        ARGPARSE_FLAG("-r", "--rainbow", &isColorModeRainbow),
        ARGPARSE_FLAG("-m", "--monochrome", &isColorModeMonochrome),
        ARGPARSE_FLAG("-b", "--basic", &isColorModeBasic),
        ARGPARSE_COLOR("-c", "--color", &lineColor),
        // ARGPARSE_INT(NULL, "--colorvariation", &colorVariation, 0, 255),
        ARGPARSE_FLAG(NULL, "--english", &english),
        ARGPARSE_END
    };

    argparse(argc, argv, params, true);
    if (isHelp) {
        printf(
            "Usage: %s ...\n"
            "Show a fullscreen matrix style scrolling effect\n"
            "\n"
            "\t-h --help             Show this message\n"
            "\t-r --rainbow          Use rainbow colors\n"
            "\t-m --monochrome       Use monochrome colors\n"
            "\t-b --basic            Only use the 16 basic colors\n"
            "\t-c --color <c>        Color of lines, e.g. #00ff00, green, or 255,0,0 (default #64ff64)\n"
            // "\t--colorvariation <i>  How much each color component can vary (default 30)\n"
            // "\t--linelength <i>      How long on average each line should be (default: 15)\n"
            "\t-d --delay <f>        Delay in seconds between frames (default: 0.05)\n"
            // "\t--density <f>         Ratio of space that should be filled with lines (default 0.5)\n"
            "\t--english             Use english characters instead of japanese\n"
            "\n",
            argv[0]
        );
        return 0;
    }

    if (isColorModeRainbow + isColorModeMonochrome + isColorModeBasic > 1) {
        fprintf(stderr, "Only one of -r, -m or -b can be specified\n");
        return 1;
    } else if (isColorModeRainbow) {
        colorMode = COLOR_MODE_RAINBOW;
    } else if (isColorModeMonochrome) {
        colorMode = COLOR_MODE_MONOCHROME;
    } else if (isColorModeBasic) {
        colorMode = COLOR_MODE_BASIC;
    } else {
        colorMode = COLOR_MODE_NORMAL;
    }

    // density = lineLength / (lineLength + lineGap)
    lineGap = (lineLength / density) - lineLength;

    randInit();
    termInit(handleSig);
    // Predraw so we start not blank
    handleResize();
    for (int x = 0; x < termSize.w; ++x)
        fillCol(x);
    fflush(stdout);
    while (true) {
        usleep((unsigned int)(1e6 * frameDelay));
        if (interrupted)
            break;
        if (termWaitForUnpause())
            redrawAll();
        const char c = termReadNonblock();
        if (c == ' ') {
            while (termReadNonblock() != ' ')
                usleep((unsigned int)(1e6 * frameDelay / 2));
        } else if (c == 'j') {
            english = !english;
            redrawAll();
        }
        handleResize();
        for (int x = 0; x < termSize.w; ++x) {
            if (lineGaps[x] > lineGap && randGet(0, 1) == 1) {
                const size_t index = lineNew(x);
                if (index == (size_t) - 1) break;
                const Line* line = &lines[index];
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
