#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct __attribute__((packed, aligned(4))) {
	uint8_t type;
	uint8_t r;
	uint8_t g;
	uint8_t b;
} AnsiiColor;

static inline AnsiiColor ansiiRGB(uint8_t r, uint8_t g, uint8_t b) {
	return (AnsiiColor){1, r, g, b};
}

static inline AnsiiColor ansii256(uint8_t v) {
	return (AnsiiColor){2, v, 0, 0};
}

static inline void ansiiColor(AnsiiColor color, bool bg) {
	if (color.type == 1) {
		printf("\x1b[%c8;2;%u;%u;%um", bg ? '4' : '3', color.r, color.g,
			   color.b);
	} else {
		if (color.r <= 7) {
			printf("\x1b[%c%cm", bg ? '4' : '3', '0' + color.r);
		} else if (color.r >= 8 && color.r <= 15) {
			if (bg)
				printf("\x1b[10%cm", '0' + color.r);
			else
				printf("\x1b[9%cm", '0' + color.r);
		} else {
			printf("\x1b[%c8;5;%um", bg ? '4' : '3', color.r);
		}
	}
}

static inline void ansiiFg(AnsiiColor color) { ansiiColor(color, false); }
static inline void ansiiBg(AnsiiColor color) { ansiiColor(color, true); }

static inline void ansiiReset() { fputs("\x1b[0m", stdout); }
static inline void ansiiResetFg() { fputs("\x1b[39m", stdout); }
static inline void ansiiResetBg() { fputs("\x1b[49m", stdout); }
static inline void ansiiClearScreen() { fputs("\x1b[2J", stdout); }
static inline void ansiiClearCursorToStart() { fputs("\x1b[1J", stdout); }
static inline void ansiiBold() { fputs("\x1b[1m", stdout); }

static inline void ansiiMove(int x, int y) {
	printf("\x1b[%d;%dH", y + 1, x + 1);
}
