#pragma once

#include <stddef.h>

typedef struct {
	char _;
} ArgparseParamNone;

typedef struct {
	int minLength;
	int maxLength;
} ArgparseParamString;

typedef struct {
	int min;
	int max;
} ArgparseParamInt;

typedef struct {
	float min;
	float max;
} ArgparseParamFloat;

typedef struct {
	unsigned char r, g, b;
} ArgparseColor;

typedef struct {
	const char *const *choices;
} ArgparseParamChoice;

typedef union {
	ArgparseParamNone n;
	ArgparseParamString s;
	ArgparseParamInt i;
	ArgparseParamFloat f;
	ArgparseParamChoice c;
} ArgparseParamUnion;

typedef struct {
	const char *shortName;
	const char *longName;
	void *ptr;
	int type;
	ArgparseParamUnion data;
} ArgparseParam;

#define ARGPARSE_TYPE_END -1
#define ARGPARSE_TYPE_FLAG 0
#define ARGPARSE_TYPE_STRING 1
#define ARGPARSE_TYPE_INT 2
#define ARGPARSE_TYPE_FLOAT 3
#define ARGPARSE_TYPE_COLOR 4
#define ARGPARSE_TYPE_CHOICE 5

#define ARGPARSE_END                                                           \
	((ArgparseParam){NULL, NULL, NULL, ARGPARSE_TYPE_END,                      \
					 (ArgparseParamUnion){.n = (ArgparseParamNone){}}})

#define ARGPARSE_FLAG(shortName, longName, ptr)                                \
	((ArgparseParam){shortName, longName, (void *)(bool *)ptr,                 \
					 ARGPARSE_TYPE_FLAG,                                       \
					 (ArgparseParamUnion){.n = (ArgparseParamNone){}}})

#define ARGPARSE_HELP(ptr) ARGPARSE_FLAG("-h", "--help", ptr)

#define ARGPARSE_STRING(shortName, longName, ptr, minLength, maxLength)        \
	((ArgparseParam){shortName, longName, (void *)(const char **)ptr,          \
					 ARGPARSE_TYPE_STRING,                                     \
					 (ArgparseParamUnion){                                     \
						 .s = (ArgparseParamString){minLength, maxLength}}})

#define ARGPARSE_INT(shortName, longName, ptr, min, max)                       \
	((ArgparseParam){shortName, longName, (void *)(int *)ptr,                  \
					 ARGPARSE_TYPE_INT,                                        \
					 (ArgparseParamUnion){.i = (ArgparseParamInt){min, max}}})

#define ARGPARSE_FLOAT(shortName, longName, ptr, min, max)                     \
	((ArgparseParam){                                                          \
		shortName, longName, (void *)(float *)ptr, ARGPARSE_TYPE_FLOAT,        \
		(ArgparseParamUnion){.f = (ArgparseParamFloat){min, max}}})

#define ARGPARSE_COLOR(shortName, longName, ptr)                               \
	((ArgparseParam){shortName, longName, (void *)(ArgparseColor *)ptr,        \
					 ARGPARSE_TYPE_COLOR,                                      \
					 (ArgparseParamUnion){.n = (ArgparseParamNone){}}})

#define ARGPARSE_CHOICE(shortName, longName, ptr, choices)                     \
	((ArgparseParam){                                                          \
		shortName, longName, (void *)(int *)ptr, ARGPARSE_TYPE_CHOICE,         \
		(ArgparseParamUnion){.c = (ArgparseParamChoice){choices}}})

void argparse(int argc, char *const *argv, const ArgparseParam *params,
			  bool strict);
