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

typedef union {
    ArgparseParamNone n;
    ArgparseParamString s;
    ArgparseParamInt i;
    ArgparseParamFloat f;
} ArgparseParamUnion;

typedef struct {
    const char* shortName;
    const char* longName;
    void* ptr;
    int type;
    ArgparseParamUnion data;
} ArgparseParam;

#define ARGPARSE_TYPE_END -1
#define ARGPARSE_TYPE_FLAG 0
#define ARGPARSE_TYPE_STRING 1
#define ARGPARSE_TYPE_INT 2
#define ARGPARSE_TYPE_FLOAT 3

#define ARGPARSE_END ((ArgparseParam){NULL, NULL, NULL, ARGPARSE_TYPE_END, (ArgparseParamUnion){.n = (ArgparseParamNone){}}})

#define ARGPARSE_FLAG(shortName, longName, ptr) \
    ((ArgparseParam){shortName, longName, (void*)(bool*)ptr, \
    ARGPARSE_TYPE_FLAG, (ArgparseParamUnion){.n = (ArgparseParamNone){}}})

#define ARGPARSE_HELP(ptr) ARGPARSE_FLAG("-h", "--help", ptr)

#define ARGPARSE_STRING(shortName, longName, ptr, minLength, maxLength) \
    ((ArgparseParam){shortName, longName, (void*)(const char**)ptr, \
    ARGPARSE_TYPE_STRING, (ArgparseParamUnion){.s = (ArgparseParamString){minLength, maxLength}}})

#define ARGPARSE_INT(shortName, longName, ptr, min, max) \
    ((ArgparseParam){shortName, longName, (void*)(int*)ptr, \
    ARGPARSE_TYPE_INT, (ArgparseParamUnion){.i = (ArgparseParamInt){min, max}}})

#define ARGPARSE_FLOAT(shortName, longName, ptr, min, max) \
    ((ArgparseParam){shortName, longName, (void*)(float*)ptr, \
    ARGPARSE_TYPE_FLOAT, (ArgparseParamUnion){.f = (ArgparseParamFloat){min, max}}})

void argparse(int argc, char* const* argv, const ArgparseParam* params);
