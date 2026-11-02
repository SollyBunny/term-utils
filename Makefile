CFLAGS = -Wall -Wextra -Wpedantic -fsanitize=address -g -fno-omit-frame-pointer

all: matrix lolcat

matrix: src/matrix.c src/lib/term.* src/lib/rand.* src/lib/argparse.*
	cc $(CFLAGS) $(filter %.c,$^) -o $@

lolcat: src/lolcat.c src/lib/term.* src/lib/rand.* src/lib/argparse.*
	cc $(CFLAGS) $(filter %.c,$^) -o $@ -lm
