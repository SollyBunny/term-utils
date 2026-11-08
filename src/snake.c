#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/rand.h"
#include "lib/term.h"
#include "lib/text.h"

static const int fruitValue = 5;
static const int graceFrames = 3;
static const int initialSnakeLen = 30;
static const float delaySeconds = 0.05;
static const bool loop = true;

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

// A pattern snake colors (255 color)
static unsigned int snakeColors[9] = {21, 20, 19, 18, 17, 18, 19, 20, 21};

// Occilating fruit color (255 color)
static unsigned int fruitColors[5] = {196, 197, 198, 197, 196};

static char *deathMsgs[18] = {
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
		for (unsigned int i = 0; i < snakeLen; ++i) {
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
	for (unsigned int i = 3; i < snakeLen - 1; ++i) {
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

static bool play() {
	// Create snake
	snakeLen = initialSnakeLen;
	snake = realloc(snake, snakeLen * sizeof(snake[0]));
	snake[0] = (Pos){sizeX / 2, sizeY / 2};
	for (unsigned int i = 1; i < snakeLen;
		 ++i) // Set all snake positions to spawn point
		snake[i] = snake[0];

	// Create fruit
	placeFruit();

	// Draw first frame of board
	printf("\x1b[0m\x1b[2J");
	printf("\x1b[%d;%dH\x1b[48;5;33m  ", snake[0].y + 1,
		   snake[0].x * 2 + 1); // draw snake head
	printf("\x1b[%d;%dH\x1b[48;5;%um  ", fruitPos.y + 1, fruitPos.x * 2 + 1,
		   fruitColors[0]); // draw fruit
	fflush(stdout);

	unsigned int t = 0;
	int grace = 0;		// Grace frames
	Pos dir = DIR_NONE; // dx and dy
	while (true) {
		sleepStable(delaySeconds);
		if (termInterrupted)
			destroy();

		termWaitForUnpause();

		t++;

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

		// Move all snake peices along by one
		for (unsigned int i = snakeLen - 1; i > 0; --i)
			snake[i] = snake[i - 1];
		snake[0] = snakeNew; // Insert new snake peice
		grace = 0; // Set grace to 0 because the snake moved, so assuming it
				   // hasn't collided with anything

		// Fruit collision
		if (posEqual(snakeNew, fruitPos)) {
			if (snakeLen + fruitValue >= (size_t)(sizeX * sizeY))
				return true;

			placeFruit();

			// Add new snake bits
			snakeLen += fruitValue;
			snake = realloc(snake, snakeLen * sizeof(snake[0]));
			for (unsigned int i = snakeLen - fruitValue; i < snakeLen; ++i)
				snake[i] = snakeNew;
		}

		printf("\x1b[0m\x1b[2J");

		// Draw info
		printf("\x1b[%d;0HPOS %04d %04d", sizeY + 1, snake[0].x, snake[0].y);
		printf("\x1b[%d;%dH%05d SCORE", sizeY + 1,
			   sizeX * 2 + 1 - (int)sizeof(" SCORE") - 5, (int)snakeLen);

		// Draw snake
		printf("\x1b[%d;%dH\x1b[48;5;33m  ", snake[0].y + 1,
			   snake[0].x * 2 + 1);
		for (unsigned int i = 1; i < snakeLen; ++i)
			printf("\x1b[%d;%dH\x1b[48;5;%um  ", snake[i].y + 1,
				   snake[i].x * 2 + 1,
				   snakeColors[(i + (t / 2)) %
							   (sizeof(snakeColors) / sizeof(snakeColors[0]))]);

		// Draw fruit
		printf("\x1b[%d;%dH\x1b[48;5;%um  ", fruitPos.y + 1, fruitPos.x * 2 + 1,
			   fruitColors[t % (sizeof(fruitColors) / sizeof(fruitColors[0]))]);

		fflush(stdout); // since no \n are written, stdout must be flushed or
						// "lag" will be seen
	}
}

int main(void) {
	randInit();
	termInit();

	const TermSize *termSize = termSizeUpdate();
	sizeX = termSize->w / 2;
	sizeY = termSize->h - 1;

	while (true) {
		const bool win = play();

		if (win) {
			printf("\x1b[0m\x1b[%d;0HYou Win! Want to restart? (y/n)",
				   sizeY + 1);
			printf("\x1b[42m");
			drawText(termSize->w / 2, termSize->h / 2, "YOU WIN!", true);
		} else {
			printf("\x1b[0m\x1b[%d;0HYou Died! Want to restart? (y/n)",
				   sizeY + 1);
			printf("\x1b[41m");
			drawText(termSize->w / 2, termSize->h / 2, "YOU DIED!", true);
			const char *deathMsg = deathMsgs[randGet(
				0, sizeof(deathMsgs) / sizeof(deathMsgs[0]) - 1)];
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
