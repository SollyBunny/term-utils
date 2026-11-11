#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "lib/ansii.h"
#include "lib/rand.h"
#include "lib/term.h"

static const uint8_t TILE_COLORS[] = {0, 21, 28, 124, 17, 52, 32, 255, 240};

enum State {
	STATE_NONE = 0,
	STATE_UNCOVERED,
	STATE_FLAG,
};

typedef struct {
	// 9 == bomb, 0-8 == number of bombs next to this tile
	uint8_t bomb;
	enum State state;
} Tile;
static Tile *grid;
static int sizeX, sizeY;
static int cursorX, cursorY;
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
	ansiiReset();
	ansiiMove(x * 2, y);
	switch (tile->state) {
	case STATE_NONE:
		ansiiBg(ansii256(cursor ? 231 : 237));
		fputs("  ", stdout);
		break;
	case STATE_UNCOVERED:
		ansiiBold();
		if (cursor)
			ansiiBg(ansii256(231));
		if (tile->bomb == 0) {
			fputs("  ", stdout);
		} else if (tile->bomb >= 9) {
			ansiiFg(ansii256(196));
			fputs("ꓘk", stdout);
		} else {
			ansiiFg(ansii256(TILE_COLORS[tile->bomb]));
			printf("%u ", tile->bomb);
		}
		break;
	case STATE_FLAG:
		ansiiBold();
		ansiiBg(ansii256(cursor ? 231 : 237));
		ansiiFg(ansii256(196));
		fputs("|>", stdout);
		break;
	}
}

static void drawAll() {
	ansiiBg(ansii256(237));
	ansiiMove(sizeX * 2, sizeY + 1);
	ansiiClearCursorToStart();
	for (int y = 0; y < sizeY; ++y) {
		for (int x = 0; x < sizeX; ++x) {
			if (!isState(x, y, STATE_NONE))
				drawTile(x, y, false);
		}
	}
	drawTile(cursorX, cursorY, true);
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
	termSetSettings(
		(TermSettings){.lower = true, .readBlocking = true, .readTime = 0});

	TermSize termSize = termGetSize();
	sizeY = termSize.h;
	sizeX = termSize.w / 2;

	cursorX = sizeX / 2;
	cursorY = sizeY / 2;

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

	ansiiBg(ansii256(237));
	ansiiMove(sizeX * 2, sizeY + 1);
	ansiiClearCursorToStart();
	drawTile(cursorX, cursorY, true);
	fflush(stdout);

	bool wasTooSmall = false;
	while (true) {
		// Read input
		TermEvent event = termGetEvent();
		int key = 0;
		if (event.type == TERM_EVENT_INTERUPT) {
			destroy();
		} else if (event.type == TERM_EVENT_RESIZED) {
			termSize = event.data.resized;
		} else if (event.type == TERM_EVENT_PAUSED) {
			drawAll();
			continue;
		} else if (event.type == TERM_EVENT_KEY) {
			key = event.data.key;
		}

		if (sizeX * 2 > termSize.w || sizeY > termSize.h) {
			wasTooSmall = true;

			const char line1[] = "Terminal too small";
			char line2[64];
			const int line2Size = snprintf(
				line2, sizeof(line2), "needs %d x %d", sizeX * 2, sizeY + 1);

			ansiiReset();
			ansiiClearScreen();
			ansiiMove(termSize.w / 2 - (int)sizeof(line1) / 2, termSize.h / 2);
			fputs(line1, stdout);
			ansiiMove(termSize.w / 2 - line2Size / 2, termSize.h / 2 + 1);
			fputs(line2, stdout);
			fflush(stdout);
			continue;
		} else if (wasTooSmall) {
			wasTooSmall = false;
			drawAll();
		}

		// Clear last cursor
		drawTile(cursorX, cursorY, false);

		if (key == 'w' || key == TERM_CHAR_UP) {
			cursorY -= 1;
		} else if (key == 's' || key == TERM_CHAR_DOWN) {
			cursorY += 1;
		} else if (key == 'd' || key == TERM_CHAR_RIGHT) {
			cursorX += 1;
		} else if (key == 'a' || key == TERM_CHAR_LEFT) {
			cursorX -= 1;
		} else if (key == 'q') {
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
						maxBombsAround -=
							isState(cursorX + dx, cursorY + dy, STATE_FLAG);
					}
				}
				if (maxBombsAround == 0) {
					for (int dy = -1; dy <= 1; ++dy) {
						for (int dx = -1; dx <= 1; ++dx) {
							if (dx == 0 && dy == 0)
								continue;
							if (isState(cursorX + dx, cursorY + dy,
										STATE_NONE)) {
								if (grid[(cursorY + dy) * sizeX + cursorX + dx]
										.bomb == 9) {
									dead = true;
									destroy();
								} else {
									recurseFind(cursorX + dx, cursorY + dy,
												false);
								}
							}
						}
					}
				}
			}
		} else if (key == 'e') {
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
						uncoveredAround += !isState(cursorX + dx, cursorY + dy,
													STATE_UNCOVERED);
					}
				}
				if (uncoveredAround == (int)tile->bomb) {
					for (int dy = -1; dy <= 1; ++dy) {
						for (int dx = -1; dx <= 1; ++dx) {
							if (dx == 0 && dy == 0)
								continue;
							if (isState(cursorX + dx, cursorY + dy,
										STATE_NONE)) {
								grid[(cursorY + dy) * sizeX + cursorX + dx]
									.state = STATE_FLAG;
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
		fflush(stdout);
	}
}
