
// Imports
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// Key codes
#define BREAK 3

#define KEYW 119
#define KEYA 97
#define KEYS 115
#define KEYD 100

// Config
#define GRACE 3
#define SNAKELEN 5
#define SPEED 60000
#define FRUITVAL 5
#define LOOP

// turn on for debug
//#define DEBUG

// size of the board (terminal)
unsigned int sizex; // termwidth / 2
unsigned int sizey; // termheight - 1
unsigned int size; // sizex * sizey

// position of fruit = y * sizex + x
unsigned int fruitpos;

// a list of positions where the snake is
// position = fruit = y * sizex + x
unsigned int * snake;
unsigned int snakelen;

// A pattern snake colors (255color)
unsigned int snakecol[9] = {21, 20, 19, 18, 17, 18, 19, 20, 21};
unsigned int snakecollen = 9;

// Occilating fruit colour (255color)
unsigned int fruitcol[5] = {196, 197, 198, 197, 196};
unsigned int fruitcollen = 5;

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
unsigned int deathmsgsize[18] = {
	18, 21, 19, 34, 22, 51, 10, 4, 3, 10, 11, 40, 37, 33, 17, 22, 6, 30
};
unsigned int deathmsglen = 18;



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

	/*
	for (unsigned char y = 0; y < sy; ++y) {
		//printf("%u: ", y);
		for (unsigned char x = 0; x < sx; ++x) {
			//printf("%u, ", img[y][x]);

			if ((&img)[y][x] == 0) {
				if (x == 0) {
					++x;
					goto drawflipchr;
				}
				break;
			}

			for (unsigned int i = 0; i < (&img)[y][x]; ++i) {
				putchar(' ');
				putchar(' ');
			}
			++x; // iterate loop
			if (!(x < sx))
				break;

			if ((&img)[y][x] == 0)
				break;

			drawflipchr:
			printf("\x1b[%uC", (*img)[y][x] * 2);

		}
		printf("\x1b[%u;%uH", (sizey - sy) / 2 + y + 1, sizex - sx - 1);
	}*/


}

// util function to get base 10 intiger "size"
unsigned int intsize(unsigned int a) {
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

// Used instead of getchar, it will return 0 if theres no character pressed, otherwise returned pressed
char _waiting;
char sgetchar() {
	ioctl(0, FIONREAD, &_waiting);
	if (_waiting == 0) return 0;
	return getchar();
}

/*void print() { // refresh snake grid

	fputs("\x1b[H\x1b[2J\x1b[?25l\x1b[48;5;0m", stdout);

	unsigned int nl = 0;
	for (unsigned int i = 0; i < size; ++i) {

		for (unsigned int m = 0; m < snakelen; ++m) { // check if i is in snake
			if (i == snake[m]) {
				printf("\x1b[48;5;%um  \x1b[45;5;0m", snakecol[(m + (t / 2)) % snakecollen]);
				//putchar('S');
				goto afterchecks;
			}
		}

		if (i == fruitpos) {
			//putchar('F');
			printf("\x1b[48;5;%um  \x1b[45;5;0m", fruitcol[(t) % fruitcollen]);
		} else {
			putchar(' ');
			putchar(' ');
		}

		afterchecks:

		++nl;
		if (nl >= sizex) {
			nl = 0;
			putchar('\n');
		}

	} // this code renders the whole playing feild, but if im only rendering 2 tiles whats the point

}*/

// Randomly place the fruit on the board where there's a free tile
void placefruit() { placefruitstart:
	fruitpos = rand() % size;
	for (unsigned int i = 0; i < snakelen; ++i) { // checking if theres a collision
		if (snake[i] == fruitpos) goto placefruitstart; // go back to the start, choose another location
	}
}


int main(void) {

	#ifdef DEBUG
		sizey = 5;
		sizex = 30;
	#else
		struct winsize _w; // get term size
		ioctl(0, TIOCGWINSZ, &_w);
		sizey = _w.ws_row - 1;
		sizex = _w.ws_col / 2;
	#endif

	size = sizex * sizey;
	// the sizes of these numbers will be used to display info neatly
	unsigned int sizes = intsize(sizex); // sizes used as temp var here
	unsigned int sizess = intsize(sizey);
	if (sizes > sizess) sizess = sizes; // get the largest x/y intsize
	sizess++; // padding
	sizes = intsize(size);

	srand(time(0)); // set random seed
	start: _waiting = 0;

	placefruit();

	// create snake
	snakelen = SNAKELEN;
	snake = malloc(snakelen * sizeof(unsigned int));
	#ifdef DEBUG
		snake[0] = 0;
	#else
		snake[0] = (size + sizex) / 2;
	#endif
	for (unsigned int i = 1; i < snakelen - 1; ++i) { // set all snake positions to spawn point
		snake[i] = snake[0];
	}

	// set terminal settins (dont echo characters / queue stdin / ignore ctrl-c )
	//system("/bin/stty -echo -ixon -icanon time 5 min 1 intr undef");
	struct termios term, restore;
	tcgetattr(0, &term);
	tcgetattr(0, &restore); // backup the original terminal state to restore later
	term.c_lflag &= ~(ICANON|ECHO|~ISIG);
	tcsetattr(0, TCSANOW, &term);


	// draw first frame of board
	/*fputs("\x1b[48;5;0m\x1b[H\x1b[2J\x1b[?25l", stdout); // clear, hide cursor
	printf("\x1b[%u;%uH\x1b[48;5;33m  ", snake[0] / sizex + 1, snake[0] % sizex * 2 + 1); // draw snake head
	printf("\x1b[%u;%uH\x1b[48;5;%um  ", fruitpos / sizex + 1, fruitpos % sizex * 2 + 1, fruitcol[0]); // draw fruit*/
	printf("\x1b[2J\x1b[?25l\x1b[%u;%uH\x1b[48;5;33m  \x1b[%u;%uH\x1b[48;5;%um  ",
		snake[0] / sizex + 1, snake[0] % sizex * 2 + 1,
		fruitpos / sizex + 1, fruitpos % sizex * 2 + 1, fruitcol[0]
	);
	fflush(stdout);
	//goto dead;

	// mainloop
	char c;
	char dir = 0; // the direction of the snake (stored by keycode of last key), used to make sure snake doesnt double back on itself
	int grace = 0; // stores the amount of grace frames had, when a deadly collision happens this will be iterated, if it goes over a threshold, youll die
	unsigned int newsnake;
	while ((c = sgetchar()) != BREAK) {

		mainloopkeys:
			switch (c) {
				case KEYW:
					if (dir == KEYS) goto mainloopkeydefault; // if tried to double back, ignore input
					newsnake = snake[0] - sizex;
					if (newsnake >= size) { // top/bottom bounds check
						#ifdef LOOP
							newsnake += size;
						#else
							goto mainloopgracecheck;
						#endif
					}
					break;
				case KEYS:
					if (dir == KEYW) goto mainloopkeydefault;
					newsnake = snake[0] + sizex;
					if (newsnake >= size) { // top/bottom bounds check
						#ifdef LOOP
							newsnake -= size;
						#else
							goto mainloopgracecheck;
						#endif
					}
					break;
				case KEYA:
					if (dir == KEYD) goto mainloopkeydefault;
					newsnake = snake[0] - 1;
					if ((newsnake / sizex) != (snake[0] / sizex)) { // left rgiht bound detection
						#ifdef LOOP
							newsnake += sizex;
						#else
							goto mainloopgracecheck;
						#endif
					}
					break;
				case KEYD:
					if (dir == KEYA) goto mainloopkeydefault;
					newsnake = snake[0] + 1;
					if ((newsnake / sizex) != (snake[0] / sizex)) { // left rgiht bound detection
						#ifdef LOOP
							newsnake -= sizex;
						#else
							goto mainloopgracecheck;
						#endif
					}
					break;
				default: mainloopkeydefault:
					if (dir == 0)
						goto mainloopcontinue;
					c = dir;
					goto mainloopkeys;
			}

		for (unsigned int i = 3; i < snakelen; ++i) { // check if there is collision with self (impossible to collide with first 3 peices of snake)
			if (snake[i] == newsnake) {
				goto mainloopgracecheck;
			}
		}

		dir = c;



		printf("\x1b[%u;%uH\x1b[48;5;0m  ",  snake[snakelen - 1] / sizex + 1, snake[snakelen - 1] % sizex * 2 + 1); // remove old snake

		// move all snake peices along by one
		for (unsigned int i = snakelen - 1; i > 0; --i) {
			snake[i] = snake[i - 1];
		}
		snake[0] = newsnake; // insert new snake peice
		grace = 0; // set grace to 0 because the snake moved, so assuming it hasn't collided with anything

		// fruit collision
		if (newsnake == fruitpos) {
			//printf("\x1b[%u;%uH  ", newsnake % sizex, newsnake / sizex); // remove old fruit
			snakelen += FRUITVAL;
			if (snakelen >= size) // if snakelen == size then that means the whole board is filled with snake, so you win
				goto win;

			placefruit();
			snake = realloc(snake, snakelen * sizeof(unsigned int)); // make snaek bigger
			for (unsigned int i = snakelen - FRUITVAL; i < snakelen; ++i) {
				snake[i] = newsnake; // set new snake bits to extend snaek (shoudnt effect colours, due to the head being different color)
			}
		}

		printf("\x1b[%u;%uH\x1b[48;5;33m  ", snake[0] / sizex + 1, snake[0] % sizex * 2 + 1); // draw snaek head
		for (unsigned int i = 1; i < snakelen; ++i) { // draw snake body
			printf("\x1b[%u;%uH\x1b[48;5;%um  ", snake[i] / sizex + 1, snake[i] % sizex * 2 + 1, snakecol[(i + (t / 2)) % snakecollen]);
		}
		printf("\x1b[%u;%uH\x1b[48;5;%um  ", fruitpos / sizex + 1, fruitpos % sizex * 2 + 1, fruitcol[(t) % fruitcollen]); // draw fruit

		// update info
		#ifdef DEBUG // if debug is on, also show the key pressed
			printf("\x1b[0m\x1b[%d;0H\x1b[0mPOS: %u SCORE: %5u Key: %c", snake[0], snakelen, c);
		#else
			printf("\x1b[0m\x1b[%d;0H\x1b[0mPOS %0*u,%0*u%*s%0*u SCORE",
				sizey * 2,
				sizess, snake[0] % sizex,
				sizess, snake[0] / sizex + 1,
				(sizex * 2) - (4 + sizess + 1 + sizess + 7 + sizes), "", // i realy hope compiler optimizes this
				sizes, snakelen
			);
		#endif

		fflush(stdout); // since no \n are written, stdout must be flushed or "lag" will be seen

		mainloopcontinue:
		usleep(SPEED); // the frame happens almost instantly so delay a bit (this is very inconsistnet if the frame takes alot of time, but this is c, this runs almost instantly)

		t++; // iterate time
	}
	goto quit; // only reached if ctrl-c is pressed

	end:
		do { // wait for valid input, if y reset, otherwise quit
			c = getchar();
			//putchar(c);
		} while (c != 121 && c != 110 && c != BREAK); // 121 = y, 110 = n
		if (c == 121)
			goto start;

		// reset stuff & exit
		quit:
			fputs("\x1b[?25h\x1b[0m\x1b[2J\x1b[H", stdout); // clear, show cursor, goto home position, assume color has been reset
			tcsetattr(0, TCSANOW, &restore); // restore terminal state
			free(snake);
			return 0;

	dead:
		//fputs("You died!\n", stdout);
		printf("\x1b[0m\x1b[%d;0HYou Died! Want to restart? (y/n)", sizey * 2);
		draw(&deathimg);
		unsigned int i = rand() % deathmsglen;
		printf("\x1b[0m\x1b[%d;%dH%s",
			sizey / 2 + 3,
			sizex - (deathmsgsize[i] / 2),
			deathmsg[i]
		);
		goto end;

	win:
		//fputs("You win!\n", stdout);
		printf("\x1b[0m\x1b[%d;0HYou Win! Want to restart? (y/n)", sizey * 2);
		draw(&winimg);
		goto end;

	mainloopgracecheck:
		if (dir != c) goto mainloopkeydefault; // if changing dir would result in _instant_ death, dont change dir
		if (grace == GRACE) goto dead;
		grace++;
		goto mainloopcontinue;

}
