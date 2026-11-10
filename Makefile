PREFIX ?= /usr/local/bin

CFLAGS = -Wall -Wextra -Wpedantic
# CFLAGS += -fsanitize=address -g -fno-omit-frame-pointer
CFLAGS += -O3

build: matrix pipes lolcat snake minesweeper

matrix: src/matrix.c src/lib/term.* src/lib/rand.* src/lib/argparse.*
	cc $(CFLAGS) $(filter %.c,$^) -o $@ -lm

pipes: src/pipes.c src/lib/term.* src/lib/rand.* src/lib/argparse.*
	cc $(CFLAGS) $(filter %.c,$^) -o $@ -lm

lolcat: src/lolcat.c src/lib/rand.* src/lib/argparse.*
	cc $(CFLAGS) $(filter %.c,$^) -o $@ -lm

snake: src/snake.c src/lib/term.* src/lib/rand.* src/lib/argparse.* src/lib/text.* src/lib/circ.*
	cc $(CFLAGS) $(filter %.c,$^) -o $@

minesweeper: src/minesweeper.c src/lib/term.* src/lib/rand.* src/lib/argparse.*
	cc $(CFLAGS) $(filter %.c,$^) -o $@

install: build
	install -Dm755 matrix $(PREFIX)/matrix
	install -Dm755 pipes $(PREFIX)/pipes
	install -Dm755 lolcat $(PREFIX)/lolcat
	install -Dm755 snake $(PREFIX)/snake
	install -Dm755 minesweeper $(PREFIX)/minesweeper

clean:
	rm -f matrix pipes lolcat snake minesweeper

.PHONY: build install clean
