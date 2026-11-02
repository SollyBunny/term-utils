#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "argparse.h"
#include "shared.h"

static float density;
static int lineLength;
static int lineGap;
static bool english;

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
    unsigned char r, g, b;
} Color;

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
    int delta = randGet(-15, 15);
    int result = c + delta;

    if (result < 0) result = 0;
    if (result > 255) result = 255;

    return (unsigned char)result;
}

static Color randomizeColor(Color color) {
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

// Draw the whole line
static void lineDrawWhole(const Line* line) {
    const int yStart =
        line->y >= termSize.h ? termSize.h - 1 : line->y;
    const int yEnd =
        line->y >= line->l ? line->y - line->l : 0;

    for (int y = yStart; y >= yEnd; --y) {
        const Color color = randomizeColor(line->color);
        printf(
            "\x1b[%u;%uH"
            "\x1b[38;2;%u;%u;%um"
            "%s",
            y + 1, line->x + 1,
            (unsigned)color.r, (unsigned)color.g, (unsigned)color.b,
            randomChar()
        );
    }
}

// Draw just the head and clear the tail
static void lineDraw(const Line* line) {
    // Draw the character if inside the terminal.
    if (line->y < termSize.h && line->x < termSize.w) {
        const Color color = randomizeColor(line->color);
        printf(
            "\x1b[%u;%uH"
            "\x1b[38;2;%u;%u;%um"
            "%s",
            line->y + 1, line->x + 1,
            (unsigned)color.r, (unsigned)color.g, (unsigned)color.b,
            randomChar()
        );
    }

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
    // Make new col
    const int l = lineLength + randGet(0, 4);
    lines[i] = (Line){
        true, x, 0, l, (Color){100, 255, 100}
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
    const TermSize* termSizeNew = termSizeUpdate();
    if (termSizeNew == NULL)
        return;
    // Handle line size changing
    const size_t linesAllocedNew = (size_t)((float)termSizeNew->w * (float)termSizeNew->h * density * 1.5f);
    lines = realloc(lines, sizeof(*lines) * linesAllocedNew);
    if (!lines)
        abort();
    for (size_t i = linesAlloced; i < linesAllocedNew; ++i)
        lines[i] = (Line){}; // Clear new lines
    lineGaps = realloc(lineGaps, sizeof(*lineGaps) * termSizeNew->w);
    // Destroy old cols
    const size_t maxOldLineIndex = linesAlloced > linesAllocedNew ? linesAllocedNew : linesAlloced;
    if (termSizeNew->w < termSize.w) {
        for (size_t i = 0; i < maxOldLineIndex; ++i) {
            Line* line = &lines[i];
            if (line->active && line->x >= termSizeNew->w)
                line->active = false;
        }
    }
    // Fill new cols
    for (int x = termSize.w; x < termSizeNew->w; ++x) {
        lineGaps[x] = 0;
        fillCol(x);
    }
    // Destroy old rows
    if (termSizeNew->h < termSize.h) {
        for (size_t i = 0; i < maxOldLineIndex; ++i) {
            Line* line = &lines[i];
            if (line->active && line->y > termSize.h + line->l)
                line->active = false;
        }
    }
    // Fill new rows
    if (termSizeNew->h > termSize.h) {
        // Needs correct termSize
        const TermSize termSizeOld = termSize;
        termSize = *termSizeNew;
        for (size_t i = 0; i < maxOldLineIndex; ++i) {
            Line* line = &lines[i];
            if (line->active)
                continue;
            if (line->y >= termSizeOld.h)
                lineDrawWhole(line);
        }
        termSize = termSizeOld;
    }
    // Apply vars
    linesAlloced = linesAllocedNew;
    termSize = *termSizeNew;
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
    lineLength = 15;
    density = 0.5;
    float secondsPerFrame = 0.05;
    bool isHelp;
    ArgparseParam params[] = {
        ARGPARSE_HELP(&isHelp),
        ARGPARSE_FLAG(NULL, "--english", &english),
        ARGPARSE_INT(NULL, "--linelength", &lineLength, 2, 100),
        ARGPARSE_FLOAT(NULL, "--wait", &secondsPerFrame, 0, 1),
        ARGPARSE_FLOAT(NULL, "--density", &density, 0.1, 0.9),
        ARGPARSE_END
    };

    argparse(argc, argv, params);
    if (isHelp) {
        printf(
            "Usage: %s ...\n"
            "Show a fullscreen matrix style scrolling effect\n"
            "\n"
            "\t-h --help         Show this message\n"
            "\t--english         Use english characters instead of japanese\n"
            "\t--linelength <i>  How long on average each line should be (default: 15)\n"
            "\t--wait <f>        Seconds to wait between frames (default: 0.05)\n"
            "\t--density <f>     Ratio of space that should be filled with lines (default 0.5)\n"
            "\n",
            argv[0]
        );
        return 0;
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
        usleep((unsigned int)(1e6 * secondsPerFrame));
        if (interrupted)
            break;
        if (termWaitForUnpause())
            redrawAll();
        const char c = termReadNonblock();
        if (c == ' ') {
            while (termReadNonblock() != ' ')
                usleep((unsigned int)(1e6 * secondsPerFrame / 2));
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
