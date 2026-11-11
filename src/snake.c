#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/ansii.h"
#include "lib/argparse.h"
#include "lib/circ.h"
#include "lib/rand.h"
#include "lib/term.h"
#include "lib/text.h"

typedef ArgparseColor Color;

static Color snakeColor;

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

// <Pos>
static CircBuffer snake;

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
		for (unsigned i = 0; i < snake.len; ++i) {
			if (posEqual(CIRC_INDEX(snake, Pos, i), fruitPos)) {
				collided = true;
				break;
			}
		}
		if (!collided)
			return;
	}
}

static bool doCollisionWithSnakeAndBorder(Pos *pos) {
	if (pos->x < 0 || pos->x >= sizeX) {
		if (loop)
			pos->x = (pos->x % sizeX + sizeX) % sizeX;
		else
			return true;
	} else if (pos->y < 0 || pos->y >= sizeY) {
		if (loop)
			pos->y = (pos->y % sizeY + sizeY) % sizeY;
		else
			return true;
	}

	// Impossible to collide with first 3 parts
	// Allow colliding into last part as it will move by the time we get there
	for (size_t i = 3; i < snake.len - 1; ++i) {
		if (posEqual(CIRC_INDEX(snake, Pos, i), *pos))
			return true;
	}

	return false;
}

static void destroy() {
	termDestroy();
	circFree(&snake);
	exit(0);
}

static uint8_t colorsRainbow(int i) {
	static uint8_t colors[] = {196, 202, 208, 214, 220, 226, 190, 154, 118,
							   82,	46,	 48,  49,  50,	51,	 45,  39,  33,
							   27,	21,	 57,  93,  129, 165, 201};
	return colors[i % (sizeof(colors) / sizeof(colors[0]))];
}

static uint8_t colorsMonochrome(int i) {
	const uint8_t b = 238;
	const uint8_t w = 255;
	i = i % ((w - b) * 2);
	return b + (i <= (w - b) ? i : (w - b) * 2 - i);
}

static uint8_t clampedAddReflect(unsigned char a, int b) {
	const int res = (int)a + b;
	if (res < 0)
		return -res;
	if (res > 255)
		return 255 - (res - 255);
	return res;
}

static void draw() {
	AnsiiColor colorOld = {};
	// Draw snake
	for (int i = snake.len - 1; i >= 0; --i) {
		AnsiiColor color;
		const Pos *pos = &CIRC_INDEX(snake, Pos, i);
		const int j = i / fruitValue;
		switch (snakeColor.mode) {
		case ARGPARSE_COLOR_MODE_NORMAL:
		case ARGPARSE_COLOR_MODE_RANDOM: {
			const int d = j % 2 == 0 ? -20 : 20;
			uint8_t r, g, b;
			if (snakeColor.mode == ARGPARSE_COLOR_MODE_NORMAL) {
				r = clampedAddReflect(snakeColor.r, d);
				g = clampedAddReflect(snakeColor.g, d);
				b = clampedAddReflect(snakeColor.b, d);
			} else if (snakeColor.mode == ARGPARSE_COLOR_MODE_RANDOM) {
				unsigned h = snake.len / fruitValue;
				h ^= h + 0x9e3779b9u + (h << 6) + (h >> 2);
				h ^= h >> 16;
				h *= 0x85ebca6bu;
				h ^= h >> 13;
				r = clampedAddReflect(h % 255, d);
				g = clampedAddReflect(h / 255 % 255, d);
				b = clampedAddReflect(h / 255 / 255 % 255, d);
			}
			color = ansiiRGB(r, g, b);
			break;
		}
		case ARGPARSE_COLOR_MODE_RAINBOW:
			color = ansii256(colorsRainbow(j));
			break;
		case ARGPARSE_COLOR_MODE_MONOCHROME:
			color = ansii256(colorsMonochrome(j));
			break;
		case ARGPARSE_COLOR_MODE_BASIC:
			color = ansii256(j % 2 == 0 ? 10 : 11);
			break;
		}
		ansiiMove(pos->x * 2, pos->y);
		if (!ansiiColorEq(color, colorOld)) {
			colorOld = color;
			ansiiBg(color);
		}
		fputs("  ", stdout);
	}

	// Draw fruit
	AnsiiColor color;
	switch (snakeColor.mode) {
	case ARGPARSE_COLOR_MODE_NORMAL:
	case ARGPARSE_COLOR_MODE_RANDOM: {
		unsigned char r, g, b;
		if (snakeColor.mode == ARGPARSE_COLOR_MODE_NORMAL) {
			r = snakeColor.r;
			g = snakeColor.g;
			b = snakeColor.b;
		} else if (snakeColor.mode == ARGPARSE_COLOR_MODE_RANDOM) {
			unsigned h = snake.len;
			h ^= h + 0x9e3779b9u + (h << 6) + (h >> 2);
			h ^= h >> 16;
			h *= 0x85ebca6bu;
			h ^= h >> 13;
			r = h % 255;
			g = h / 255 % 255;
			b = h / 255 / 255 % 255;
		}
		color = ansiiRGB(r, g, b);
		break;
	}
	case ARGPARSE_COLOR_MODE_RAINBOW:
		color = ansii256(colorsRainbow(snake.len / fruitValue));
		break;
	case ARGPARSE_COLOR_MODE_MONOCHROME:
		color = ansii256(colorsMonochrome(snake.len / fruitValue));
		break;
	case ARGPARSE_COLOR_MODE_BASIC:
		color = ansii256(9);
		break;
	}
	ansiiMove(fruitPos.x * 2, fruitPos.y);
	ansiiFg(color);
	fputs("  ", stdout);
}

static bool play() {
	// Create snake
	snake = circCreate(initialSnakeLen, sizeof(Pos));
	// Set all snake positions to spawn point
	for (int i = 0; i < initialSnakeLen; ++i)
		*((Pos *)circAddFront(&snake)) = (Pos){sizeX / 2, sizeY / 2};

	// Create fruit
	placeFruit();

	int grace = 0;		// Grace frames
	Pos dir = DIR_NONE; // dx and dy
	while (true) {
		sleepStable(delaySeconds);
		TermEvent event = termGetEvent();
		Pos newDir = dir;
		if (event.type == TERM_EVENT_INTERUPT) {
			destroy();
		} else if (event.type == TERM_EVENT_RESIZED) {
			/* TODO */
		} else if (event.type == TERM_EVENT_PAUSED) {
			newDir = dir = DIR_NONE;
		} else if (event.type == TERM_EVENT_KEY) {
			newDir = charToDir(event.data.key);
			if (posEqual(newDir, posFlip(dir)))
				newDir = dir;
		}

		if (!posEqual(newDir, DIR_NONE)) {
			bool collided = false;
			Pos snakeNew = posAdd(CIRC_INDEX(snake, Pos, 0), newDir);

			if (doCollisionWithSnakeAndBorder(&snakeNew)) {
				// If collision was result of direction change, undo it and try
				// again
				newDir = dir;
				snakeNew = posAdd(CIRC_INDEX(snake, Pos, 0), newDir);
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
			circPopBack(&snake);
			*((Pos *)circAddFront(&snake)) = snakeNew;

			// Fruit collision
			if (posEqual(snakeNew, fruitPos)) {
				if (snake.len + fruitValue >= (size_t)(sizeX * sizeY))
					return true;

				placeFruit();

				// Add new snake bits
				const Pos snakeEnd = CIRC_INDEX(snake, Pos, snake.len - 1);
				for (size_t i = 0; i < (size_t)fruitValue; ++i)
					*((Pos *)circAddBack(&snake)) = snakeEnd;
			}
		}

		const int score = ((int)snake.len - initialSnakeLen) / fruitValue;
		char scoreStr[32] = "";
		if (score > 0)
			snprintf(scoreStr, sizeof(scoreStr), "%d", score);

		ansiiReset();
		ansiiClearScreen();
		ansiiBg(ansii256(244));
		drawText(termSize.w / 2, termSize.h / 2, scoreStr, true);
		draw();
		fflush(stdout);
	}
}

int main(int argc, char **argv) {
	bool isHelp = false;
	snakeColor = (Color){0, 0, 0, ARGPARSE_COLOR_MODE_BASIC};

	ArgparseParam params[] = {ARGPARSE_HELP(&isHelp),
							  ARGPARSE_COLOR("-c", "--color", &snakeColor),
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
	termSetSettings(
		(TermSettings){.lower = true, .readBlocking = false, .readTime = 0});

	termSize = termGetSize();
	sizeX = termSize.w / 2;
	sizeY = termSize.h;

	while (true) {
		const bool win = play();

		ansiiReset();
		ansiiClearScreen();
		draw();
		if (win) {
			ansiiReset();
			ansiiMove(0, sizeY);
			fputs("You Win! Want to restart? (y/n)", stdout);
			ansiiBg(ansii256(2));
			drawText(termSize.w / 2, termSize.h / 2, "YOU WIN!", true);
		} else {
			ansiiReset();
			ansiiMove(0, sizeY);
			fputs("You Died! Want to restart? (y/n)", stdout);
			ansiiBg(ansii256(1));
			drawText(termSize.w / 2, termSize.h / 2, "YOU DIED!", true);
			const char *deathMsg = DEATH_MSGS[randGet(
				0, sizeof(DEATH_MSGS) / sizeof(DEATH_MSGS[0]) - 1)];
			ansiiReset();
			ansiiMove(sizeX - ((int)strlen(deathMsg) / 2), sizeY / 2 + 3);
			fputs(deathMsg, stdout);
		}
		fflush(stdout);

		int key = 0;
		termSetSettings(
			(TermSettings){.lower = true, .readBlocking = true, .readTime = 0});
		do { // wait for valid input, if y reset, otherwise quit
			TermEvent event = termGetEvent();
			if (event.type == TERM_EVENT_INTERUPT)
				destroy();
			else if (event.type == TERM_EVENT_KEY)
				key = event.data.key;
		} while (key != 'y' && key != 'n');
		termSetSettings((TermSettings){
			.lower = true, .readBlocking = false, .readTime = 0});
		if (key == 'n')
			break;
	}

	destroy();
}
