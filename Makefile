matrix: matrix.c shared.c shared.h argparse.c argparse.h
	cc -Wall -Wextra -Wpedantic -fsanitize=address -g shared.c matrix.c argparse.c -o matrix
