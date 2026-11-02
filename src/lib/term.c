#include <sched.h>
#include <locale.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "term.h"

#ifdef __SANITIZE_ADDRESS__
#include <sanitizer/asan_interface.h>

void __asan_on_error() {
    termDestroy();
}
#endif

static volatile atomic_bool termResized = true;
static volatile atomic_bool termPaused = false;
_Static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "atomic_bool must always be lock-free");

static struct termios termiosOriginal = {};
static bool termInited = false;
static char* termBuffer = NULL;
static void (*termHandleSig)(int);

static void handleSignal(int sig) {
    switch (sig) {
        case SIGWINCH:
            termResized = true;
            break;
        case SIGINT:
        case SIGTERM:
        case SIGQUIT:
            if (termHandleSig)
                termHandleSig(sig);
            break;
        case SIGTSTP:
            termPaused = true;
            break;
    }
}

void termInit(void (*handleSig)(int)) {
    if (termInited) {
        fputs("Inited term twice\n", stderr);
        return;
    }
    termInited = true;
    termHandleSig = handleSig;

    // Setup handlers
    struct sigaction sa = {0};
    sa.sa_handler = handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    signal(SIGWINCH, handleSignal);
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
    signal(SIGQUIT, handleSignal);
    signal(SIGTSTP, handleSignal);
    signal(SIGCONT, handleSignal);

    // Setup big stdout buffer
    termBuffer = malloc(1024 * 1024);
    setvbuf(stdout, termBuffer, _IOFBF, 1024 * 1024);

    printf(
        "\x1b[s" // Save cursor position
        "\x1b[?1049h" // Alt buffer
        "\x1b[?25l" // Hide cursor
        "\x1b[?7l" // Disable wrapping
        "\x1b[2J" // Clear screen
        "\x1b[H" // Reset cursor
    );
    fflush(stdout);

	tcgetattr(STDIN_FILENO, &termiosOriginal);
    struct termios termiosCurrent;
	tcgetattr(STDIN_FILENO, &termiosCurrent);
	// Immediate input, No echo
	termiosCurrent.c_lflag &= ~(ICANON|ECHO);
    termiosCurrent.c_cc[VMIN] = 0;
    termiosCurrent.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &termiosCurrent);
}

void termDestroy() {
    if (!termInited) {
        fputs("Destroyed term twice\n", stderr);
        return;
    }
    termInited = false;

    setlocale(LC_ALL, "");

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &termiosOriginal);

    printf(
        "\x1b[u" // Load cursor position
        "\x1b[?7h" // Enable wrapping
        "\x1b[?25h" // Show cursor
        "\x1b[?1049l" // Return to normal screen
        "\x1b[0m" // Reset text attributes (colors, bold, etc.)
    );
    fflush(stdout);

    setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
    if (termBuffer)
        free(termBuffer);
}

char termReadNonblock() {
    char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    return n == 1 ? c : 0;
}

char termRead() {
    return getchar();
}

static TermSize termSize = {-1, -1};

TermSize* termSizeUpdate() {
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
        termInit(termHandleSig);

    return true;
}
