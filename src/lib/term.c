#include <ctype.h>
#include <locale.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "term.h"

#ifdef __SANITIZE_ADDRESS__
#include <sanitizer/asan_interface.h>

void __asan_on_error() { termDestroy(); }
#endif

atomic_bool termInterrupted = false;
static atomic_bool termResized = true;
static atomic_bool termPaused = false;
_Static_assert(ATOMIC_BOOL_LOCK_FREE == 2,
			   "atomic_bool must always be lock-free");

static struct termios termiosOriginal = {};
static struct termios termiosCurrent = {};
static bool termInited = false;
static char *termBuffer = NULL;

static struct timespec sleepStableNext = {};

static void handleSignal(int sig) {
	switch (sig) {
	case SIGWINCH:
		termResized = true;
		break;
	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
		termInterrupted = true;
		break;
	case SIGTSTP:
		termPaused = true;
		break;
	}
}

void termInit() {
	if (termInited) {
		fputs("Inited term twice\n", stderr);
		return;
	}
	termInited = true;

	// Setup handlers
	struct sigaction sa = {0};
	sa.sa_handler = handleSignal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGWINCH, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);
	sigaction(SIGCONT, &sa, NULL);

	// Setup big stdout buffer
	termBuffer = malloc(1024 * 1024);
	setvbuf(stdout, termBuffer, _IOFBF, 1024 * 1024);

	printf("\x1b[s"		 // Save cursor position
		   "\x1b[?1049h" // Alt buffer
		   "\x1b[?25l"	 // Hide cursor
		   "\x1b[?7l"	 // Disable wrapping
		   "\x1b[2J"	 // Clear screen
		   "\x1b[H"		 // Reset cursor
	);
	fflush(stdout);

	tcgetattr(STDIN_FILENO, &termiosOriginal);
	tcgetattr(STDIN_FILENO, &termiosCurrent);
	// Immediate input, No echo
	termiosCurrent.c_lflag &= ~(ICANON | ECHO);
	termiosCurrent.c_cc[VMIN] = 0;
	termiosCurrent.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &termiosCurrent);

	sleepStableReset();
}

void termDestroy() {
	if (!termInited)
		return;
	termInited = false;

	setlocale(LC_ALL, "");

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &termiosOriginal);

	printf("\x1b[u"		 // Load cursor position
		   "\x1b[?7h"	 // Enable wrapping
		   "\x1b[?25h"	 // Show cursor
		   "\x1b[?1049l" // Return to normal screen
		   "\x1b[0m"	 // Reset text attributes (colors, bold, etc.)
	);
	fflush(stdout);

	setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
	if (termBuffer)
		free(termBuffer);
}

void termReadSetBlocking(bool blocking) {
	if (!termInited) {
		fprintf(stderr, "Tried to set blocking when not inited");
		return;
	}
	termiosCurrent.c_cc[VMIN] = blocking ? 1 : 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &termiosCurrent);
}

char termRead() {
	char c;
	ssize_t n = read(STDIN_FILENO, &c, 1);
	return n == 1 ? c : 0;
}

int termReadFancy(bool lower) {
	char c = termRead();
	if (c == '\x1b') {
		c = termRead();
		if (c == '[') {
			c = termRead();
			if (c == 'A')
				return TERM_CHAR_UP;
			else if (c == 'B')
				return TERM_CHAR_DOWN;
			else if (c == 'C')
				return TERM_CHAR_RIGHT;
			else if (c == 'D')
				return TERM_CHAR_LEFT;
		}
	} else {
		return lower ? tolower(c) : c;
	}
	return 0;
}

static TermSize termSize = {-1, -1};

TermSize *termSizeUpdate() {
	if (!atomic_exchange(&termResized, false))
		return NULL;
	struct winsize termSizeTemp;
	if (ioctl(0, TIOCGWINSZ, &termSizeTemp) == -1)
		return NULL;
	const int w = termSizeTemp.ws_col;
	const int h = termSizeTemp.ws_row;
	if (w == termSize.w && h == termSize.h)
		return NULL;
	termSize = (TermSize){w, h};
	return &termSize;
}

bool termWaitForUnpause() {
	if (!atomic_exchange(&termPaused, false))
		return false;

	if (termInited)
		termDestroy();

	raise(SIGSTOP);

	if (!termInited)
		termInit();

	sleepStableReset();

	return true;
}

void sleepStableReset() {
	clock_gettime(CLOCK_MONOTONIC, &sleepStableNext);
}

void sleepStable(float seconds) {
	long ns = (long)(seconds * 1e9);

	sleepStableNext.tv_sec += ns / 1000000000L;
	sleepStableNext.tv_nsec += ns % 1000000000L;

	if (sleepStableNext.tv_nsec >= 1000000000L) {
		sleepStableNext.tv_sec++;
		sleepStableNext.tv_nsec -= 1000000000L;
	}

	while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &sleepStableNext,
						   NULL) != 0 &&
		   !termInterrupted)
		;
}
