#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib/rand.h"
#include "lib/term.h"

// Config
#define GRACE 3
#define SNAKELEN 30
#define SPEED 60000
#define FRUITVAL 5

static const bool loop = true;

// size of the board (terminal)
static int sizex; // termwidth / 2
static int sizey; // termheight - 1
static int size; // sizex * sizey

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
    if (c == 'w' || c == 'W')
        return dirUp;
    if (c == 's' || c == 'S')
        return dirDown;
    if (c == 'a' || c == 'A')
        return dirLeft;
    if (c == 'd' || c == 'D')
        return dirRight;
    return dirNone;
}

static inline Pos posFlip(Pos dir) {
    return (Pos){ -dir.x, -dir.y };
}

static Pos fruitpos;

static Pos* snake;
static size_t snakelen;

// A pattern snake colors (255color)
unsigned int snakecol[9] = {21, 20, 19, 18, 17, 18, 19, 20, 21};

// Occilating fruit colour (255color)
unsigned int fruitcol[5] = {196, 197, 198, 197, 196};

// A list of death msgs
char *deathmsg[18] = {
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

// A time variable
unsigned int t = 0;

// Encoded images for death/win toasts
struct Img {
	unsigned char sx; // maximun size of img
	unsigned char sy; // height of img
	unsigned char s; // length of d[]
	unsigned char d[]; // image data in format of number of blacks/number of whites
};

struct Img deathimg = {
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

struct Img winimg = {
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
void draw(struct Img *img) {
	// place the cursor in the middle minus half of the size of the image
	printf("\x1b[48;5;1m\x1b[%u;%uH", (sizey - img->sy) / 2, (sizex - img->sx) - 1);

	unsigned char x = 0;
	unsigned char y = 0;
	unsigned char state = 0; // drawing black/white
	for (unsigned char i = 0; i < img->s; ++i) {
		if (img->d[i] == 0) {
			if (x == 0) { // check if the starting black # is 0, if so start drawing whites
				state = 1;
				x++;
				i++;
				goto drawendcheck;
			}
			x = 0; // otherwise assume that the 0 means a new line
			state = 0;
			y++;
			printf("\x1b[%u;%uH", (sizey - img->sy) / 2 + y, sizex - img->sx - 1); // move cursor to start of line one down
			continue;
		}
		drawendcheck:

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

// util function to get base 10 intiger "size"
unsigned int log10i(unsigned int a) {
	if      (a < 10       ) return 1;
	else if (a < 100      ) return 2;
	else if (a < 1000     ) return 3;
	else if (a < 10000    ) return 4;
	else if (a < 100000   ) return 5;
	else if (a < 1000000  ) return 6;
	else if (a < 10000000 ) return 7;
	else if (a < 100000000) return 8;
	else                    return 10;
}

// Randomly place the fruit on the board where there's a free tile
void placefruit() {
    placefruitstart:
	fruitpos = (Pos){ randGet(0, sizex - 1), randGet(0, sizey - 1) };
	// Check if collides with snake
	for (unsigned int i = 0; i < snakelen; ++i) {
		if (posEqual(snake[i], fruitpos))
		    goto placefruitstart;
	}
}

static bool doCollisionWithSnakeAndBorder(Pos* ppos) {
    Pos pos = *ppos;
    for (unsigned int i = 3; i < snakelen; ++i) { // check if there is collision with self (impossible to collide with first 3 peices of snake)
        if (posEqual(snake[i], pos))
			return true;
    }

	if (pos.x < 0 || pos.x >= sizex) {
	    if (loop)
			ppos->x = (pos.x % sizex + sizex) % sizex;
	    else
			return true;
	} else if (pos.y < 0 || pos.y >= sizey) {
	    if (loop)
			ppos->y = (pos.y % sizey + sizey) % sizey;
	    else
			return true;
	}

	return false;
}

int main(void) {
    randInit();
    termInit();

    TermSize *termSize = termSizeUpdate();
	sizex = termSize->w / 2;
	sizey = termSize->h - 1;
	size = sizex * sizey;

	// the sizes of these numbers will be used to display info neatly
	unsigned int sizes = log10i(sizex); // sizes used as temp var here
	unsigned int sizess = log10i(sizey);
	if (sizes > sizess) sizess = sizes; // get the largest x/y intsize
	sizess++; // padding
	sizes = log10i(size);

	start:

	// create snake
	snakelen = SNAKELEN;
	snake = realloc(snake, snakelen * sizeof(snake[0]));
	snake[0] = (Pos){ sizex / 2, sizey / 2 };
	for (unsigned int i = 1; i < snakelen - 1; ++i) { // set all snake positions to spawn point
		snake[i] = snake[0];
	}

	// create fruit
	placefruit();

	// draw first frame of board
	printf("\x1b[%u;%uH\x1b[48;5;33m  ", snake[0].y + 1, snake[0].x * 2 + 1); // draw snake head
	printf("\x1b[%u;%uH\x1b[48;5;%um  ", fruitpos.y + 1, fruitpos.x * 2 + 1, fruitcol[0]); // draw fruit
	fflush(stdout);

	// mainloop
	int grace = 0; // stores the amount of grace frames had, when a deadly collision happens this will be iterated, if it goes over a threshold, youll die

	Pos dir = dirNone; // delta x and y
	while (true) {
        usleep(SPEED); // the frame happens almost instantly so delay a bit (this is very inconsistnet if the frame takes alot of time, but this is c, this runs almost instantly)
    	if (termInterrupted)
    	    goto quit;

        t++; // iterate time

	    Pos newDir = charToDir(termRead());
		// If invalid/no input or opposite input
		if (posEqual(newDir, dirNone) || posEqual(newDir, posFlip(dir))) {
		    if (posEqual(dir, dirNone)) // Can't do anything with no input;
				continue;
			newDir = dir;
		}

        bool collided = false;
		Pos newsnake = posAdd(snake[0], newDir);

		if (doCollisionWithSnakeAndBorder(&newsnake)) {
		    // If collision was result of direction change, undo it and try again
			newDir = dir;
			newsnake = posAdd(snake[0], newDir);
			collided = doCollisionWithSnakeAndBorder(&newsnake);
		}

		if (collided) {
		    grace += 1;
			if (grace >= GRACE) goto dead;
			continue;
		}

		dir = newDir;

		printf("\x1b[%u;%uH\x1b[48;5;0m  ",  snake[snakelen - 1].y + 1, snake[snakelen - 1].x * 2 + 1); // remove old snake

		// move all snake peices along by one
		for (unsigned int i = snakelen - 1; i > 0; --i) {
			snake[i] = snake[i - 1];
		}
		snake[0] = newsnake; // insert new snake peice
		grace = 0; // set grace to 0 because the snake moved, so assuming it hasn't collided with anything

		// fruit collision
		if (posEqual(newsnake, fruitpos)) {
    		if (snakelen + FRUITVAL >= (size_t)size) {
    			goto win;
            }

			placefruit();

			snakelen += FRUITVAL;
			snake = realloc(snake, snakelen * sizeof(snake[0])); // make snaek bigger
			for (unsigned int i = snakelen - FRUITVAL; i < snakelen; ++i) {
				snake[i] = newsnake; // set new snake bits to extend snaek (shoudnt effect colours, due to the head being different color)
			}
		}

		printf("\x1b[%u;%uH\x1b[48;5;33m  ", snake[0].y + 1, snake[0].x * 2 + 1); // draw snaek head
		for (unsigned int i = 1; i < snakelen; ++i) { // draw snake body
			printf("\x1b[%u;%uH\x1b[48;5;%um  ", snake[i].y + 1, snake[i].x * 2 + 1, snakecol[(i + (t / 2)) % (sizeof(snakecol) / sizeof(snakecol[0]))]);
		}
		printf("\x1b[%u;%uH\x1b[48;5;%um  ", fruitpos.y + 1, fruitpos.x * 2 + 1, fruitcol[t % (sizeof(fruitcol) / sizeof(fruitcol[0]))]); // draw fruit

		// update info
		printf("\x1b[0m\x1b[%d;0H\x1b[0mPOS %0*d,%0*d%*s%0*u SCORE",
			sizey * 2,
			sizess, snake[0].x,
			sizess, snake[0].y,
			(sizex * 2) - (4 + sizess + 1 + sizess + 7 + sizes), " ",
			sizes, (int)snakelen
		);

		fflush(stdout); // since no \n are written, stdout must be flushed or "lag" will be seen
	}

	dead:
		printf("\x1b[0m\x1b[%d;0HYou Died! Want to restart? (y/n)", sizey * 2);
		draw(&deathimg);
		unsigned int i = randGet(0, sizeof(deathmsg) / sizeof(deathmsg[0]) - 1);
		printf("\x1b[0m\x1b[%d;%dH%s",
			sizey / 2 + 3,
			sizex - ((int)strlen(deathmsg[i]) / 2),
			deathmsg[i]
		);
		fflush(stdout);
		goto end;

	win:
		printf("\x1b[0m\x1b[%d;0HYou Win! Want to restart? (y/n)", sizey * 2);
		draw(&winimg);
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
