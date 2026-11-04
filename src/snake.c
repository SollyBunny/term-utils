#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib/rand.h"
#include "lib/term.h"

static const int fruitValue = 5;
static const int graceFrames = 3;
static const int initialSnakeLen = 30;
static const float delaySeconds = 0.05;
static const bool loop = true;

// size of the board (terminal)
static int sizeX; // termSize.x / 2
static int sizeY; // termSize.y - 1

typedef struct {
    int x, y;
} Pos;

static inline bool posEqual(Pos a, Pos b) {
    return a.x == b.x && a.y == b.y;
}

static inline Pos posAdd(Pos a, Pos b) {
    return (Pos){ a.x + b.x, a.y + b.y };
}

static const Pos dirUp    = { 0, -1 };
static const Pos dirDown  = { 0,  1 };
static const Pos dirLeft  = {-1,  0 };
static const Pos dirRight = { 1,  0 };
static const Pos dirNone  = { 0,  0 };

static Pos charToDir(char c) {
    if (c == 'w' || c == 'W') return dirUp;
    if (c == 's' || c == 'S') return dirDown;
    if (c == 'a' || c == 'A') return dirLeft;
    if (c == 'd' || c == 'D') return dirRight;
    return dirNone;
}

static inline Pos posFlip(Pos dir) {
    return (Pos){ -dir.x, -dir.y };
}

static Pos fruitPos;

static Pos* snake;
static size_t snakeLen;

// A pattern snake colors (255 color)
static unsigned int snakeColors[9] = {21, 20, 19, 18, 17, 18, 19, 20, 21};

// Occilating fruit color (255 color)
static unsigned int fruitColors[5] = {196, 197, 198, 197, 196};

static char* deathMsgs[18] = {
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
	"You almost beat your highscore"
};

// Encoded images for death/win toasts
typedef struct {
	unsigned char sx; // maximun size of img
	unsigned char sy; // height of img
	unsigned char s; // length of d[]
	unsigned char d[]; // image data in format of number of blacks/number of whites
} Img;

static const Img deathImg = {
    .sx = 23,
    .sy = 4,
    .s = 77,
    .d = {
        1, 1, 1, 1, 3, 1, 1, 1, 1, 3, 2, 2, 3, 1, 3, 1, 2, 0,
        3, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 2, 1, 2, 3, 1, 1, 1, 1, 0,
        0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 2, 1, 2, 1, 3, 1, 1, 1, 0,
        3, 1, 3, 1, 3, 3, 2, 2, 3, 1, 3, 1, 2
    }
};

static const Img winImg = {
    .sx = 23,
    .sy = 4,
    .s = 81,
    .d = {
        1, 1, 1, 1, 3, 1, 1, 1, 1, 3, 1, 3, 1, 1, 3, 1, 2, 1, 1, 0,
        3, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 2, 0,
        0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 2, 0,
        3, 1, 3, 1, 3, 3, 2, 1, 2, 1, 3, 1, 1, 2, 1
    }
};

// Draws images from Img struct
static void drawImg(const Img* img) {
	// place the cursor in the middle minus half of the size of the image
	printf("\x1b[48;5;1m\x1b[%d;%dH", (sizeY - img->sy) / 2, (sizeX - img->sx) - 1);

	unsigned char x = 0;
	unsigned char y = 0;
	unsigned char state = 0; // drawing black/white
	for (unsigned char i = 0; i < img->s; ++i) {
		if (img->d[i] == 0) {
			if (x == 0) { // check if the starting black # is 0, if so start drawing whites
				state = 1;
				x++;
				i++;
				goto drawEndCheck;
			}
			x = 0; // otherwise assume that the 0 means a new line
			state = 0;
			y++;
			printf("\x1b[%d;%dH", (sizeY - img->sy) / 2 + y, sizeX - img->sx - 1); // move cursor to start of line one down
			continue;
		}
		drawEndCheck:

		if (state == 1) { // if drawing whites, just skip that many tiles
			state = 0;
			printf("\x1b[%uC", img->d[i] * 2);
		} else { // otherwise contintually draw a tile (2 wide)
			state = 1;
			for (unsigned int m = 0; m < img->d[i]; ++m) { // could be replaced for a printf or memset/fputs to save iterators
				putchar(' ');
				putchar(' ');
			}
		}

		x++;
	}
}

// Randomly place the fruit on the board where there's a free tile
static void placeFruit() {
    placeFruitStart:
	fruitPos = (Pos){ randGet(0, sizeX - 1), randGet(0, sizeY - 1) };
	// Check if collides with snake
	for (unsigned int i = 0; i < snakeLen; ++i) {
		if (posEqual(snake[i], fruitPos))
		    goto placeFruitStart;
	}
}

static bool doCollisionWithSnakeAndBorder(Pos* ppos) {
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

int main(void) {
    randInit();
    termInit();

    const TermSize* termSize = termSizeUpdate();
	sizeX = termSize->w / 2;
	sizeY = termSize->h - 1;

	start:

	// Create snake
	snakeLen = initialSnakeLen;
	snake = realloc(snake, snakeLen * sizeof(snake[0]));
	snake[0] = (Pos){ sizeX / 2, sizeY / 2 };
	for (unsigned int i = 1; i < snakeLen; ++i) // Set all snake positions to spawn point
		snake[i] = snake[0];

	// Create fruit
	placeFruit();

	// Draw first frame of board
	printf("\x1b[%d;%dH\x1b[48;5;33m  ", snake[0].y + 1, snake[0].x * 2 + 1); // draw snake head
	printf("\x1b[%d;%dH\x1b[48;5;%um  ", fruitPos.y + 1, fruitPos.x * 2 + 1, fruitColors[0]); // draw fruit
	fflush(stdout);

	unsigned int t = 0;
	int grace = 0; // Grace frames
	Pos dir = dirNone; // dx and dy
	while (true) {
        usleep((long)(1e6 * delaySeconds));
    	if (termInterrupted)
    	    goto quit;

        t++;

	    Pos newDir = charToDir(termRead());
		// If invalid/no input or opposite input
		if (posEqual(newDir, dirNone) || posEqual(newDir, posFlip(dir))) {
		    if (posEqual(dir, dirNone)) // Can't do anything with no input;
				continue;
			newDir = dir;
		}

        bool collided = false;
		Pos snakeNew = posAdd(snake[0], newDir);

		if (doCollisionWithSnakeAndBorder(&snakeNew)) {
		    // If collision was result of direction change, undo it and try again
			newDir = dir;
			snakeNew = posAdd(snake[0], newDir);
			collided = doCollisionWithSnakeAndBorder(&snakeNew);
		}

		if (collided) {
		    grace += 1;
			if (grace >= graceFrames) goto dead;
			continue;
		}

		dir = newDir;

		// Move all snake peices along by one
		for (unsigned int i = snakeLen - 1; i > 0; --i)
			snake[i] = snake[i - 1];
		snake[0] = snakeNew; // Insert new snake peice
		grace = 0; // Set grace to 0 because the snake moved, so assuming it hasn't collided with anything

		// Fruit collision
		if (posEqual(snakeNew, fruitPos)) {
    		if (snakeLen + fruitValue >= (size_t)(sizeX * sizeY)) {
    			goto win;
            }

			placeFruit();

			// Add new snake bits
			snakeLen += fruitValue;
			snake = realloc(snake, snakeLen * sizeof(snake[0]));
			for (unsigned int i = snakeLen - fruitValue; i < snakeLen; ++i)
				snake[i] = snakeNew;
		}

		printf("\x1b[0m\x1b[2J");

		// Draw info
		printf("\x1b[%d;0HPOS %04d %04d",
			sizeY + 1,
			snake[0].x,
			snake[0].y
		);
		printf("\x1b[%d;%dH%05d SCORE",
		    sizeY + 1,
		    sizeX * 2 + 1 - (int)sizeof(" SCORE") - 5,
			(int)snakeLen
		);

		// Draw snake
		printf("\x1b[%d;%dH\x1b[48;5;33m  ", snake[0].y + 1, snake[0].x * 2 + 1);
		for (unsigned int i = 1; i < snakeLen; ++i)
			printf("\x1b[%d;%dH\x1b[48;5;%um  ", snake[i].y + 1, snake[i].x * 2 + 1, snakeColors[(i + (t / 2)) % (sizeof(snakeColors) / sizeof(snakeColors[0]))]);

		// Draw fruit
		printf("\x1b[%d;%dH\x1b[48;5;%um  ", fruitPos.y + 1, fruitPos.x * 2 + 1, fruitColors[t % (sizeof(fruitColors) / sizeof(fruitColors[0]))]);

		fflush(stdout); // since no \n are written, stdout must be flushed or "lag" will be seen
	}

	dead:
		printf("\x1b[0m\x1b[%d;0HYou Died! Want to restart? (y/n)", sizeY + 1);
		drawImg(&deathImg);
		unsigned int i = randGet(0, sizeof(deathMsgs) / sizeof(deathMsgs[0]) - 1);
		printf("\x1b[0m\x1b[%d;%dH%s",
			sizeY / 2 + 3,
			sizeX - ((int)strlen(deathMsgs[i]) / 2),
			deathMsgs[i]
		);
		fflush(stdout);
		goto end;

	win:
		printf("\x1b[0m\x1b[%d;0HYou Win! Want to restart? (y/n)", sizeY + 1);
		drawImg(&winImg);
		fflush(stdout);
		goto end;

	end:
	    termReadSetBlocking(true);
		char c;
		do { // wait for valid input, if y reset, otherwise quit
		    c = termRead();
			if (termInterrupted)
			    goto quit;
		} while (c != 'y' && c != 'n');
		termReadSetBlocking(false);
		if (c == 'y') {
		    printf("\x1b[2J");
			goto start;
		}
		goto quit;

	// reset stuff & exit
	quit:
		termDestroy();
		free(snake);
		return 0;
}
