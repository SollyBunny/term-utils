#include <stdio.h>
#include <stdlib.h>

#include "lib/rand.h"
#include "lib/term.h"

static int sizeX, sizeY;

enum State {
	STATE_NONE = 0,
	STATE_UNCOVERED,
	STATE_FLAG,
};

typedef struct {
	// 9 == bomb, 0-8 == number of bombs next to this tile
	unsigned int bomb;
	enum State state;
} Tile;
static Tile *grid;

static const unsigned int tileColors[] = {
	0,
	21,	 // 1
	28,	 // 2
	124, // 3
	17,	 // 4
	52,	 // 5
	32,	 // 6
	255, // 7
	240, // 8

};

static bool isBomb(int x, int y) {
	if (x < 0 || x >= sizeX || y < 0 || y >= sizeY)
		return false;
	const Tile *tile = &grid[y * sizeX + x];
	return tile->bomb == 9;
}

static void drawTile(int x, int y, bool cursor) {
	const int i = y * sizeX + x;
	printf("\x1b[%u;%uH", y + 1, x * 2 + 1);
	printf("\x1b[0m");
	printf("\x1b[1m");
	switch (grid[i].state) {
	case STATE_NONE:
		if (cursor)
			printf("\x1b[48;5;231m  ");
		else
			printf("\x1b[48;5;237m  ");
		break;
	case STATE_UNCOVERED:
		if (cursor)
			printf("\x1b[48;5;231m");
		else
			printf("\x1b[0m");
		if (grid[i].bomb == 0)
			printf("  ");
		else
			printf("\x1b[38;5;%um%u ", tileColors[grid[i].bomb], grid[i].bomb);
		break;
	case STATE_FLAG:
		if (cursor)
			printf("\x1b[48;5;231m");
		else
			printf("\x1b[48;5;237m");
		printf("\x1b[38;5;196m|>");
		break;
	}
	fflush(stdout);
}

static void recurseFind(int x, int y, bool cursor) {
	if (x < 0 || x >= sizeX || y < 0 || y >= sizeY)
		return;
	Tile *tile = &grid[y * sizeX + x];
	if (tile->state != STATE_NONE)
		return;
	tile->state = STATE_UNCOVERED;
	if (tile->bomb == 0) {
		recurseFind(x - 1, y - 1, false);
		recurseFind(x - 1, y, false);
		recurseFind(x - 1, y + 1, false);
		recurseFind(x, y - 1, false);
		recurseFind(x, y + 1, false);
		recurseFind(x + 1, y - 1, false);
		recurseFind(x + 1, y, false);
		recurseFind(x + 1, y + 1, false);
	}
	drawTile(x, y, cursor);
}

int main() {
	randInit();
	termInit();

	TermSize *termSize = termSizeUpdate();
	sizeY = termSize->h;
	sizeX = termSize->w / 2;

	int cursorX = sizeX / 2;
	int cursorY = sizeY / 2;

	grid = malloc(sizeX * sizeY * sizeof(grid[0]));

	for (int i = 0; i < sizeX * sizeY; ++i) {
		grid[i].bomb = (randGet(0, 10) == 0) ? 9 : 0;
		grid[i].state = STATE_NONE;
	}

	for (int y = 0; y < sizeY; ++y) {
		for (int x = 0; x < sizeX; ++x) {
			const int i = y * sizeX + x;
			if (grid[i].bomb == 9)
				continue;
			grid[i].bomb += isBomb(x - 1, y - 1);
			grid[i].bomb += isBomb(x - 1, y);
			grid[i].bomb += isBomb(x - 1, y + 1);
			grid[i].bomb += isBomb(x, y - 1);
			grid[i].bomb += isBomb(x, y + 1);
			grid[i].bomb += isBomb(x + 1, y - 1);
			grid[i].bomb += isBomb(x + 1, y);
			grid[i].bomb += isBomb(x + 1, y + 1);
		}
	}

	termReadSetBlocking(true);
	bool firstLoop = true;
	bool dead = false;
	printf("\x1b[48;5;237m"); // BG Gray
	printf("\x1b[2J\x1b[H");  // Clear screen
	printf("\x1b[0m");		  // Reset
	fflush(stdout);
	while (true) {
		// Read input
		char c = termRead();
		if (termInterrupted)
			break;

		// Clear last cursor
		if (!firstLoop)
			drawTile(cursorX, cursorY, false);
		firstLoop = false;

		if (c == 'w') {
			cursorY -= 1;
		} else if (c == 's') {
			cursorY += 1;
		} else if (c == 'a') {
			cursorX -= 1;
		} else if (c == 'd') {
			cursorX += 1;
		} else if (c == 'q') {
			const Tile *tile = &grid[cursorY * sizeX + cursorX];
			if (tile->state == STATE_NONE) {
				if (tile->bomb == 9) {
					dead = true;
					break;
				} else {
					recurseFind(cursorX, cursorY, true);
					fflush(stdout);
				}
			}
		} else if (c == 'e') {
			Tile *tile = &grid[cursorY * sizeX + cursorX];
			if (tile->state == STATE_FLAG)
				tile->state = STATE_NONE;
			else if (tile->state == STATE_NONE)
				tile->state = STATE_FLAG;
		}
		cursorX = ((cursorX % sizeX) + sizeX) % sizeX;
		cursorY = ((cursorY % sizeY) + sizeY) % sizeY;

		// Draw new cursorPos
		drawTile(cursorX, cursorY, true);
	}

	termDestroy();
	if (dead)
		printf("You died\n");
	return 0;
}
