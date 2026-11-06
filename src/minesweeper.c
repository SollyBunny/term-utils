#include <stdio.h>
#include <stdlib.h>

#include "lib/rand.h"
#include "lib/term.h"

static const unsigned int tileColors[] = {0, 21, 28, 124, 17, 52, 32, 255, 240};

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
static int sizeX, sizeY;
static bool dead = false;

static bool isBomb(int x, int y) {
	if (x < 0 || x >= sizeX || y < 0 || y >= sizeY)
		return false;
	const Tile *tile = &grid[y * sizeX + x];
	return tile->bomb == 9;
}

static bool isState(int x, int y, enum State state) {
	if (x < 0 || x >= sizeX || y < 0 || y >= sizeY)
		return false;
	const Tile *tile = &grid[y * sizeX + x];
	return tile->state == state;
}

static void drawTile(int x, int y, bool cursor) {
	const Tile *tile = &grid[y * sizeX + x];
	printf("\x1b[%u;%uH", y + 1, x * 2 + 1);
	printf("\x1b[0m");
	printf("\x1b[1m");
	switch (tile->state) {
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
		if (tile->bomb == 0)
			printf("  ");
		else if (tile->bomb == 9)
			printf("**");
		else
			printf("\x1b[38;5;%um%u ", tileColors[tile->bomb], tile->bomb);
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
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				if (dx == 0 && dy == 0)
					continue;
				recurseFind(x + dx, y + dy, false);
			}
		}
	}
	drawTile(x, y, cursor);
}

static void destroy() {
	termDestroy();
	if (dead)
		printf("You died\n");
	exit(0);
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
			Tile *tile = &grid[y * sizeX + x];
			if (tile->bomb == 9)
				continue;
			for (int dy = -1; dy <= 1; ++dy) {
				for (int dx = -1; dx <= 1; ++dx) {
					if (dx == 0 && dy == 0)
						continue;
					tile->bomb += isBomb(x + dx, y + dy);
				}
			}
		}
	}

	termReadSetBlocking(true);
	printf("\x1b[48;5;237m"); // BG Gray
	printf("\x1b[2J\x1b[H");  // Clear screen
	printf("\x1b[0m");		  // Reset
	drawTile(cursorX, cursorY, true);
	while (true) {
		// Read input
		int c = termReadFancy(true);
		if (termInterrupted)
			break;

		// Clear last cursor
		drawTile(cursorX, cursorY, false);

		if (c == 'w' || c == TERM_CHAR_UP) {
			cursorY -= 1;
		} else if (c == 's' || c == TERM_CHAR_DOWN) {
			cursorY += 1;
		} else if (c == 'd' || c == TERM_CHAR_RIGHT) {
			cursorX += 1;
		} else if (c == 'a' || c == TERM_CHAR_LEFT) {
			cursorX -= 1;
		} else if (c == 'q') {
			const Tile *tile = &grid[cursorY * sizeX + cursorX];
			if (tile->state == STATE_NONE) {
				if (tile->bomb == 9) {
					dead = true;
					destroy();
				} else {
					recurseFind(cursorX, cursorY, true);
				}
			} else if (tile->state == STATE_UNCOVERED && tile->bomb > 0) {
				int maxBombsAround = tile->bomb;
				for (int dy = -1; dy <= 1; ++dy) {
					for (int dx = -1; dx <= 1; ++dx) {
						if (dx == 0 && dy == 0)
							continue;
						maxBombsAround -= isState(cursorX + dx, cursorY + dy, STATE_FLAG);
					}
				}
				if (maxBombsAround == 0) {
					for (int dy = -1; dy <= 1; ++dy) {
						for (int dx = -1; dx <= 1; ++dx) {
							if (dx == 0 && dy == 0)
								continue;
							if (isState(cursorX + dx, cursorY + dy, STATE_NONE)) {
								if (grid[(cursorY + dy) * sizeX + cursorX + dx].bomb == 9) {
									dead = true;
									destroy();
								} else {
									recurseFind(cursorX + dx, cursorY + dy, false);
								}
							}
						}
					}
				}
			}
		} else if (c == 'e') {
			Tile *tile = &grid[cursorY * sizeX + cursorX];
			if (tile->state == STATE_FLAG) {
				tile->state = STATE_NONE;
			} else if (tile->state == STATE_NONE) {
				tile->state = STATE_FLAG;
			} else if (tile->state == STATE_UNCOVERED && tile->bomb > 0) {
				int uncoveredAround = 0;
				for (int dy = -1; dy <= 1; ++dy) {
					for (int dx = -1; dx <= 1; ++dx) {
						if (dx == 0 && dy == 0)
							continue;
						uncoveredAround += !isState(cursorX + dx, cursorY + dy, STATE_UNCOVERED);
					}
				}
				if (uncoveredAround == (int)tile->bomb) {
					for (int dy = -1; dy <= 1; ++dy) {
						for (int dx = -1; dx <= 1; ++dx) {
							if (dx == 0 && dy == 0)
								continue;
							if (isState(cursorX + dx, cursorY + dy, STATE_NONE)) {
								grid[(cursorY + dy) * sizeX + cursorX + dx].state = STATE_FLAG;
								drawTile(cursorX + dx, cursorY + dy, false);
							}
						}
					}
				}
			}
		}
		cursorX = ((cursorX % sizeX) + sizeX) % sizeX;
		cursorY = ((cursorY % sizeY) + sizeY) % sizeY;

		// Draw new cursorPos
		drawTile(cursorX, cursorY, true);
	}

	destroy();
}
