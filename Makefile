PREFIX ?= /usr/local/bin

CFLAGS = -Wall -Wextra -Wpedantic
# CFLAGS += -fsanitize=address -g -fno-omit-frame-pointer
CFLAGS += -O3

build: matrix lolcat snake

matrix: src/matrix.c src/lib/term.* src/lib/rand.* src/lib/argparse.*
	cc $(CFLAGS) $(filter %.c,$^) -o $@ -lm

lolcat: src/lolcat.c src/lib/rand.* src/lib/argparse.*
	cc $(CFLAGS) $(filter %.c,$^) -o $@ -lm

snake: src/snake.c src/lib/term.* src/lib/rand.* src/lib/argparse.*
	cc $(CFLAGS) $(filter %.c,$^) -o $@ -lm

install: build
	install -Dm755 matrix $(PREFIX)/matrix
	install -Dm755 lolcat $(PREFIX)/lolcat

clean:
	rm -f matrix lolcat

.PHONY: build install clean
