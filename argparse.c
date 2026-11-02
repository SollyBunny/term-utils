#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

#include "argparse.h"

static const ArgparseParam* findParam(
    const ArgparseParam* params,
    const char* name
) {
    for (const ArgparseParam* p = params; p->type != ARGPARSE_TYPE_END; ++p) {
        if (
            (p->shortName && strcmp(p->shortName, name) == 0) ||
            (p->longName && strcmp(p->longName, name) == 0)
        ) {
            return p;
        }
    }
    return NULL;
}

void argparse(int argc, char* const* argv, const ArgparseParam* params) {
    size_t paramCount = 0;
    while (params[paramCount].type != ARGPARSE_TYPE_END)
        ++paramCount;

    bool seen[paramCount ? paramCount : 1];
    memset(seen, 0, sizeof(seen));

    bool error = false;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        const ArgparseParam* param = findParam(params, arg);

        if (!param) {
            fprintf(stderr, "Unrecognised parameter: %s\n", arg);
            error = true;
            continue;
        }

        size_t index = (size_t)(param - params);

        if (seen[index]) {
            fprintf(stderr, "Duplicate parameter: %s\n", arg);
            error = true;

            // Consume its value if necessary.
            if (param->type != ARGPARSE_TYPE_FLAG && i + 1 < argc)
                ++i;

            continue;
        }

        seen[index] = true;

        if (param->type == ARGPARSE_TYPE_FLAG) {
            *(bool*)param->ptr = true;
            continue;
        }

        if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for parameter: %s\n", arg);
            error = true;
            continue;
        }

        const char* value = argv[++i];

        switch (param->type) {
            case ARGPARSE_TYPE_STRING: {
                int minLength = param->data.s.minLength < 0 ? 0 : param->data.s.minLength;
                int maxLength = param->data.s.maxLength < 0 ? 1024 : param->data.s.maxLength;
                size_t length = strnlen(value, maxLength + 1);

                if (length < (size_t)minLength ||
                    length > (size_t)maxLength) {
                    fprintf(stderr,
                        "Invalid length for %s (expected %d-%d characters)\n",
                        arg, minLength, maxLength
                    );
                    error = true;
                    break;
                }

                *(const char**)param->ptr = *argv;
                break;
            }

            case ARGPARSE_TYPE_INT: {
                errno = 0;
                char* end;
                long result = strtol(value, &end, 10);

                if (errno == ERANGE ||
                    end == value ||
                    *end != '\0' ||
                    result < INT_MIN ||
                    result > INT_MAX ||
                    result < param->data.i.min ||
                    result > param->data.i.max) {

                    fprintf(stderr,
                        "Invalid integer for %s: %s\n",
                        arg, value
                    );
                    error = true;
                    break;
                }

                *(int*)param->ptr = (int)result;
                break;
            }

            case ARGPARSE_TYPE_FLOAT: {
                errno = 0;
                char* end;
                float result = strtof(value, &end);

                if (errno == ERANGE ||
                    end == value ||
                    *end != '\0' ||
                    !isfinite(result) ||
                    result < param->data.f.min ||
                    result > param->data.f.max) {

                    fprintf(stderr,
                        "Invalid float for %s: %s\n",
                        arg, value
                    );
                    error = true;
                    break;
                }

                *(float*)param->ptr = result;
                break;
            }

            default:
                fprintf(stderr, "Unknown parameter type: %d\n", param->type);
                error = true;
                break;
        }
    }

    if (error)
        exit(1);
}
