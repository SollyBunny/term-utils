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

static TermSettings termSettings;
static TermSize termSize = {0, 0};

static bool cleanupRegistered = false;

static void cleanup() {
	termDestroy();
	fclose(stdout);
	free(termBuffer);
}

static void handleSignal(int sig) {
	switch (sig) {
	case SIGWINCH:
		atomic_store(&termResized, true);
		break;
	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
		atomic_store(&termInterrupted, true);
		break;
	case SIGTSTP:
		atomic_store(&termPaused, true);
		break;
	}
}

void termInit() {
	if (termInited) {
		fputs("Inited term twice\n", stderr);
		abort();
	}
	termInited = true;

	if (!cleanupRegistered) {
		cleanupRegistered = true;
		atexit(cleanup);
	}

	// Setup handlers
	struct sigaction sa = {0};
	sa.sa_handler = handleSignal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1 ||
		sigaction(SIGTERM, &sa, NULL) == -1 ||
		sigaction(SIGQUIT, &sa, NULL) == -1 ||
		sigaction(SIGWINCH, &sa, NULL) == -1 ||
		sigaction(SIGTSTP, &sa, NULL) == -1 ||
		sigaction(SIGCONT, &sa, NULL) == -1) {
		perror("sigaction");
		abort();
	}

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
	// Give signals please
	termiosCurrent.c_lflag |= ISIG;
	// Immediate input, No echo
	termiosCurrent.c_lflag &= ~(ICANON | ECHO);
	termiosCurrent.c_cc[VMIN] = termSettings.readBlocking ? 1 : 0;
	termiosCurrent.c_cc[VTIME] = (int)(termSettings.readTime * 10);
	tcsetattr(STDIN_FILENO, TCSANOW, &termiosCurrent);

	setlocale(LC_ALL, "");

	sleepStableReset();
}

void termDestroy() {
	if (!termInited)
		return;
	termInited = false;

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &termiosOriginal);

	printf("\x1b[u"		 // Load cursor position
		   "\x1b[?7h"	 // Enable wrapping
		   "\x1b[?25h"	 // Show cursor
		   "\x1b[?1049l" // Return to normal screen
		   "\x1b[0m"	 // Reset text attributes (colors, bold, etc.)
	);
	fflush(stdout);
}

void termSetSettings(TermSettings settings) {
	if (termInited) {
		if (settings.readBlocking != termSettings.readBlocking ||
			settings.readTime != termSettings.readTime) {
			termiosCurrent.c_cc[VMIN] = settings.readBlocking ? 1 : 0;
			termiosCurrent.c_cc[VTIME] = (int)(settings.readTime * 10);
			tcsetattr(STDIN_FILENO, TCSANOW, &termiosCurrent);
		}
	}
	termSettings = settings;
}

TermSize termGetSize() {
	struct winsize termSizeTemp;
	if (ioctl(0, TIOCGWINSZ, &termSizeTemp) == -1)
		abort();
	const int w = termSizeTemp.ws_col;
	const int h = termSizeTemp.ws_row;
	termSize = (TermSize){w, h};
	return termSize;
}

static char getChar() {
	char c;
	ssize_t n = read(STDIN_FILENO, &c, 1);
	return n == 1 ? c : 0;
}

TermEvent termGetEvent() {
	if (termInterrupted)
		return (TermEvent){TERM_EVENT_INTERUPT, {}};
	if (atomic_exchange(&termResized, false)) {
		struct winsize termSizeTemp;
		if (ioctl(0, TIOCGWINSZ, &termSizeTemp) != -1) {
			const int w = termSizeTemp.ws_col;
			const int h = termSizeTemp.ws_row;
			if (w != termSize.w || h != termSize.h) {
				termSize = (TermSize){w, h};
				return (TermEvent){TERM_EVENT_RESIZED, {.resized = termSize}};
			}
		}
	}
	if (atomic_exchange(&termPaused, false)) {
		if (termInited)
			termDestroy();
		raise(SIGSTOP);
		if (!termInited)
			termInit();
		sleepStableReset();
		return (TermEvent){TERM_EVENT_PAUSED, {}};
	}

	int key = 0;
	char c = getChar();
	if (c == '\x1b') {
		c = getChar();
		if (c == '[') {
			c = getChar();
			if (c == 'A')
				key = TERM_CHAR_UP;
			else if (c == 'B')
				key = TERM_CHAR_DOWN;
			else if (c == 'C')
				key = TERM_CHAR_RIGHT;
			else if (c == 'D')
				key = TERM_CHAR_LEFT;
		}
	} else {
		key = termSettings.lower ? tolower(c) : c;
	}

	if (termInterrupted)
		return (TermEvent){TERM_EVENT_INTERUPT, {}};

	if (key > 0)
		return (TermEvent){TERM_EVENT_KEY, {.key = key}};
	else
		return (TermEvent){TERM_EVENT_NONE, {}};
}

void sleepStableReset() { clock_gettime(CLOCK_MONOTONIC, &sleepStableNext); }

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
