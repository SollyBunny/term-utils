matrix: shared.c matrix.c
	cc -Wall -Wextra -Wpedantic -fsanitize=address -g shared.c matrix.c -o matrix
