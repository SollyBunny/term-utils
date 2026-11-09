#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/argparse.h"
#include "lib/rand.h"
#include "lib/term.h"
#include "lib/text.h"

typedef ArgparseColor Color;

static Color color;

static const int fruitValue = 5;
static const int graceFrames = 3;
static const int initialSnakeLen = 30;
static const float delaySeconds = 0.05;
static const bool loop = true;

static TermSize termSize;
static int sizeX;
static int sizeY;

typedef struct {
	int x, y;
} Pos;

static inline bool posEqual(Pos a, Pos b) { return a.x == b.x && a.y == b.y; }

static inline Pos posAdd(Pos a, Pos b) { return (Pos){a.x + b.x, a.y + b.y}; }

static const Pos DIR_UP = {0, -1};
static const Pos DIR_DOWN = {0, 1};
static const Pos DIR_LEFT = {-1, 0};
static const Pos DIR_RIGHT = {1, 0};
static const Pos DIR_NONE = {0, 0};

static Pos charToDir(int c) {
	if (c == 'w' || c == TERM_CHAR_UP)
		return DIR_UP;
	if (c == 's' || c == TERM_CHAR_DOWN)
		return DIR_DOWN;
	if (c == 'd' || c == TERM_CHAR_RIGHT)
		return DIR_RIGHT;
	if (c == 'a' || c == TERM_CHAR_LEFT)
		return DIR_LEFT;
	return DIR_NONE;
}

static inline Pos posFlip(Pos dir) { return (Pos){-dir.x, -dir.y}; }

static Pos fruitPos;

static Pos *snake;
static size_t snakeLen;

static char *DEATH_MSGS[18] = {
	"Try avoiding walls",
	"Better luck next time",
	"Death is inevitable",
	"Failure is only a path to succsess",
	"Practise makes perfect",
	"When will you learn, your actions have consequences",
	"Stop dying",
	"Lmao",
	"Lol",
	"Try harder",
	"gg ez no re",
	"What doesn't kill you makes you stronger",
	"Death ends a life, not a relationship",
	"I'm so sorry to hear of your loss",
	"God bless 'merica",
	"My deepest condolences",
	"alt-f4",
	"You almost beat your highscore"};

static void placeFruit() {
	while (true) {
		fruitPos = (Pos){randGet(0, sizeX - 1), randGet(0, sizeY - 1)};
		bool collided = false;
		for (unsigned i = 0; i < snakeLen; ++i) {
			if (posEqual(snake[i], fruitPos)) {
				collided = true;
				break;
			}
		}
		if (!collided)
			return;
	}
}

static bool doCollisionWithSnakeAndBorder(Pos *ppos) {
	const Pos pos = *ppos;
	// Impossible to collide with first 3 parts
	// Allow colliding into last part as it will move by the time we get there
	for (unsigned i = 3; i < snakeLen - 1; ++i) {
		if (posEqual(snake[i], pos))
			return true;
	}

	if (pos.x < 0 || pos.x >= sizeX) {
		if (loop)
			ppos->x = (pos.x % sizeX + sizeX) % sizeX;
		else
			return true;
	} else if (pos.y < 0 || pos.y >= sizeY) {
		if (loop)
			ppos->y = (pos.y % sizeY + sizeY) % sizeY;
		else
			return true;
	}

	return false;
}

static void destroy() {
	termDestroy();
	if (snake)
		free(snake);
	exit(0);
}

static int colorsRainbow(int i) {
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

static unsigned char clampedAddReflect(unsigned char a, int b) {
	const int res = (int)a + b;
	if (res < 0)
		return -res;
	if (res > 255)
		return 255 - (res - 255);
	return res;
}

static void draw() {
	// Draw snake
	for (unsigned i = 0; i < snakeLen; ++i) {
		printf("\x1b[%d;%dH", snake[i].y + 1, snake[i].x * 2 + 1);
		const int pos = i / fruitValue;
		switch (color.mode) {
		case ARGPARSE_COLOR_MODE_NORMAL:
		case ARGPARSE_COLOR_MODE_RANDOM: {
			const int d = pos % 2 == 0 ? -20 : 20;
			unsigned char r, g, b;
			if (color.mode == ARGPARSE_COLOR_MODE_NORMAL) {
				r = clampedAddReflect(color.r, d);
				g = clampedAddReflect(color.g, d);
				b = clampedAddReflect(color.b, d);
			} else if (color.mode == ARGPARSE_COLOR_MODE_RANDOM) {
				unsigned h = snakeLen / fruitValue;
			    h ^= h + 0x9e3779b9u + (h << 6) + (h >> 2);
			    h ^= h >> 16;
			    h *= 0x85ebca6bu;
			    h ^= h >> 13;
				r = clampedAddReflect(h % 255, d);
				g = clampedAddReflect(h / 255 % 255, d);
				b = clampedAddReflect(h / 255 / 255 % 255, d);
			}
			printf("\x1b[48;2;%u;%u;%um  ", (unsigned)r, (unsigned)g,
				   (unsigned)b);
			break;
		}
		case ARGPARSE_COLOR_MODE_RAINBOW:
		case ARGPARSE_COLOR_MODE_MONOCHROME:
			printf("\x1b[48;5;%dm  ", color.mode == ARGPARSE_COLOR_MODE_RAINBOW ? colorsRainbow(pos) : colorsMonochrome(pos));
			break;
		case ARGPARSE_COLOR_MODE_BASIC:
			printf("\x1b[10%cm  ", pos % 2 == 0 ? '2' : '3');
			break;
		}
	}

	// Draw fruit
	printf("\x1b[%d;%dH", fruitPos.y + 1, fruitPos.x * 2 + 1);
	switch (color.mode) {
	case ARGPARSE_COLOR_MODE_NORMAL:
	case ARGPARSE_COLOR_MODE_RANDOM: {
		unsigned char r, g, b;
		if (color.mode == ARGPARSE_COLOR_MODE_NORMAL) {
			r = color.r;
			g = color.g;
			b = color.b;
		} else if (color.mode == ARGPARSE_COLOR_MODE_RANDOM) {
			unsigned h = snakeLen;
		    h ^= h + 0x9e3779b9u + (h << 6) + (h >> 2);
		    h ^= h >> 16;
		    h *= 0x85ebca6bu;
		    h ^= h >> 13;
			r = h % 255;
			g = h / 255 % 255;
			b = h / 255 / 255 % 255;
		}
		printf("\x1b[48;2;%u;%u;%um  ", (unsigned)r, (unsigned)g,
			   (unsigned)b);
		break;
	}
	case ARGPARSE_COLOR_MODE_RAINBOW:
	case ARGPARSE_COLOR_MODE_MONOCHROME:
		printf("\x1b[48;5;%dm  ", color.mode == ARGPARSE_COLOR_MODE_RAINBOW ? colorsRainbow(snakeLen / fruitValue) : colorsMonochrome(snakeLen / fruitValue));
		break;
	case ARGPARSE_COLOR_MODE_BASIC:
		printf("\x1b[101m  ");
		break;
	}
}

static bool play() {
	// Create snake
	snakeLen = initialSnakeLen;
	snake = realloc(snake, snakeLen * sizeof(snake[0]));
	snake[0] = (Pos){sizeX / 2, sizeY / 2};
	for (unsigned i = 1; i < snakeLen;
		 ++i) // Set all snake positions to spawn point
		snake[i] = snake[0];

	// Create fruit
	placeFruit();

	// Draw first frame of board
	printf("\x1b[0m\x1b[2J");
	draw();
	fflush(stdout);

	int grace = 0;		// Grace frames
	Pos dir = DIR_NONE; // dx and dy
	while (true) {
		sleepStable(delaySeconds);
		if (termInterrupted)
			destroy();

		termWaitForUnpause();

		Pos newDir = charToDir(termReadFancy(true));
		// If invalid/no input or opposite input
		if (posEqual(newDir, DIR_NONE) || posEqual(newDir, posFlip(dir))) {
			if (posEqual(dir, DIR_NONE)) // Can't do anything with no input;
				continue;
			newDir = dir;
		}

		bool collided = false;
		Pos snakeNew = posAdd(snake[0], newDir);

		if (doCollisionWithSnakeAndBorder(&snakeNew)) {
			// If collision was result of direction change, undo it and try
			// again
			newDir = dir;
			snakeNew = posAdd(snake[0], newDir);
			collided = doCollisionWithSnakeAndBorder(&snakeNew);
		}

		if (collided) {
			grace += 1;
			if (grace >= graceFrames)
				return false;
			continue;
		}

		dir = newDir;
		grace = 0;

		// Move all snake peices along by one
		for (unsigned i = snakeLen - 1; i > 0; --i)
			snake[i] = snake[i - 1];
		snake[0] = snakeNew; // Insert new snake peice

		// Fruit collision
		if (posEqual(snakeNew, fruitPos)) {
			if (snakeLen + fruitValue >= (size_t)(sizeX * sizeY))
				return true;

			placeFruit();

			// Add new snake bits
			snakeLen += fruitValue;
			snake = realloc(snake, snakeLen * sizeof(snake[0]));
			for (unsigned i = snakeLen - fruitValue; i < snakeLen; ++i)
				snake[i] = snakeNew;
		}

		const int score = (int)((snakeLen - initialSnakeLen) / fruitValue);
		char scoreStr[32] = "";
		if (score > 0)
			snprintf(scoreStr, sizeof(scoreStr), "%d", score);

		printf("\x1b[0m\x1b[2J");
		printf("\x1b[48;5;244m");
		drawText(termSize.w / 2, termSize.h / 2, scoreStr, true);
		draw();
		fflush(stdout);
	}
}

int main(int argc, char **argv) {
	bool isHelp = false;
	color = (Color){0, 0, 0, ARGPARSE_COLOR_MODE_BASIC};

	ArgparseParam params[] = {ARGPARSE_HELP(&isHelp),
							  ARGPARSE_COLOR("-c", "--color", &color),
							  ARGPARSE_END};

	argparse(argc, argv, params, NULL);

	if (isHelp) {
		printf("Usage: %s ...\n"
			   "Play snake in your terminal\n"
			   "\n"
			   "\t-h --help             Show this message\n"
			   "\t-c --color <c>        RRGGBB, RGB, rrr;ggg;bbb\n"
			   "\t                      monochrome, rainbow, random, basic\n"
			   "\n",
			   argv[0]);
		return 0;
	}

	randInit();
	termInit();

	termSize = *termSizeUpdate();
	sizeX = termSize.w / 2;
	sizeY = termSize.h;

	while (true) {
		const bool win = play();

		printf("\x1b[0m\x1b[2J");
		draw();
		if (win) {
			printf("\x1b[0m\x1b[%d;0HYou Win! Want to restart? (y/n)",
				   sizeY + 1);
			printf("\x1b[42m");
			drawText(termSize.w / 2, termSize.h / 2, "YOU WIN!", true);
		} else {
			printf("\x1b[0m\x1b[%d;0HYou Died! Want to restart? (y/n)",
				   sizeY + 1);
			printf("\x1b[41m");
			drawText(termSize.w / 2, termSize.h / 2, "YOU DIED!", true);
			const char *deathMsg = DEATH_MSGS[randGet(
				0, sizeof(DEATH_MSGS) / sizeof(DEATH_MSGS[0]) - 1)];
			printf("\x1b[0m\x1b[%d;%dH%s", sizeY / 2 + 4,
				   sizeX - ((int)strlen(deathMsg) / 2), deathMsg);
		}
		fflush(stdout);

		char c;
		termReadSetBlocking(true);
		do { // wait for valid input, if y reset, otherwise quit
			c = termRead();
			if (termInterrupted)
				destroy();
		} while (c != 'y' && c != 'n');
		termReadSetBlocking(false);
		if (c == 'n')
			break;
	}

	destroy();
}
