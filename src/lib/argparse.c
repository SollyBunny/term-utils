#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argparse.h"

static const ArgparseParam *findParam(const ArgparseParam *params,
									  const char *name) {
	for (const ArgparseParam *p = params; p->type != ARGPARSE_TYPE_END; ++p) {
		if ((p->shortName && strcmp(p->shortName, name) == 0) ||
			(p->longName && strcmp(p->longName, name) == 0)) {
			return p;
		}
	}
	return NULL;
}

static int hexDigit(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static bool parseHexColor(const char *value, ArgparseColor *color) {
	if (*value == '#')
		++value;

	const size_t length = strlen(value);

	if (length == 3) {
		const int r = hexDigit(value[0]);
		const int g = hexDigit(value[1]);
		const int b = hexDigit(value[2]);

		if (r < 0 || g < 0 || b < 0)
			return false;

		*color = (ArgparseColor){
			(uint8_t)(r * 17), (uint8_t)(g * 17),
			(uint8_t)(b * 17), ARGPARSE_COLOR_MODE_NORMAL};
		return true;
	}

	if (length == 6) {
		const int r1 = hexDigit(value[0]);
		const int r2 = hexDigit(value[1]);
		const int g1 = hexDigit(value[2]);
		const int g2 = hexDigit(value[3]);
		const int b1 = hexDigit(value[4]);
		const int b2 = hexDigit(value[5]);

		if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0)
			return false;

		*color = (ArgparseColor){
			(uint8_t)(r1 * 16 + r2), (uint8_t)(g1 * 16 + g2),
			(uint8_t)(b1 * 16 + b2), ARGPARSE_COLOR_MODE_NORMAL};
		return true;
	}

	return false;
}

static bool parseDecimalColor(const char *value, ArgparseColor *color) {
	unsigned components[3];

	for (int i = 0; i < 3; ++i) {
		if (*value < '0' || *value > '9')
			return false;

		unsigned component = 0;
		while (*value >= '0' && *value <= '9') {
			component = component * 10 + (unsigned)(*value - '0');
			if (component > 255)
				return false;
			++value;
		}

		components[i] = component;

		if (i < 2) {
			if (*value != ',' && *value != ';')
				return false;
			++value;
		}
	}

	if (*value != '\0')
		return false;

	*color = (ArgparseColor){
		(uint8_t)components[0], (uint8_t)components[1],
		(uint8_t)components[2], ARGPARSE_COLOR_MODE_NORMAL};
	return true;
}

static bool parseColorName(const char *value, ArgparseColor *color) {
	static const struct {
		const char *name;
		ArgparseColor color;
	} colors[] = {{"red", {255, 0, 0, ARGPARSE_COLOR_MODE_NORMAL}},
				  {"orange", {255, 165, 0, ARGPARSE_COLOR_MODE_NORMAL}},
				  {"yellow", {255, 255, 0, ARGPARSE_COLOR_MODE_NORMAL}},
				  {"green", {0, 255, 0, ARGPARSE_COLOR_MODE_NORMAL}},
				  {"blue", {0, 0, 255, ARGPARSE_COLOR_MODE_NORMAL}},
				  {"purple", {128, 0, 128, ARGPARSE_COLOR_MODE_NORMAL}},
				  {"black", {0, 0, 0, ARGPARSE_COLOR_MODE_NORMAL}},
				  {"white", {255, 255, 255, ARGPARSE_COLOR_MODE_NORMAL}},
				  {"magenta", {255, 0, 255, ARGPARSE_COLOR_MODE_NORMAL}},
				  {"rainbow", {0, 0, 0, ARGPARSE_COLOR_MODE_RAINBOW}},
				  {"monochrome", {0, 0, 0, ARGPARSE_COLOR_MODE_MONOCHROME}},
				  {"random", {0, 0, 0, ARGPARSE_COLOR_MODE_RANDOM}},
				  {"basic", {0, 0, 0, ARGPARSE_COLOR_MODE_BASIC}}};

	for (size_t i = 0; i < sizeof(colors) / sizeof(*colors); ++i) {
		const char *a = value;
		const char *b = colors[i].name;

		while (*a && *b) {
			char ca = *a;
			char cb = *b;

			if (ca >= 'A' && ca <= 'Z')
				ca += 'a' - 'A';
			if (cb >= 'A' && cb <= 'Z')
				cb += 'a' - 'A';

			if (ca != cb)
				break;

			++a;
			++b;
		}

		if (*a == '\0' && *b == '\0') {
			*color = colors[i].color;
			return true;
		}
	}

	return false;
}

static bool parseColor(const char *value, ArgparseColor *color) {
	if (parseHexColor(value, color))
		return true;
	if (parseDecimalColor(value, color))
		return true;
	return parseColorName(value, color);
}

void argparse(int argc, char *const *argv, const ArgparseParam *params,
			  bool *consumed) {
	size_t paramCount = 0;
	while (params[paramCount].type != ARGPARSE_TYPE_END)
		++paramCount;

	bool seen[paramCount ? paramCount : 1];
	memset(seen, false, sizeof(seen));

	bool error = false;

	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];

		const ArgparseParam *param = findParam(params, arg);

		if (!param) {
			if (consumed)
				continue;
			fprintf(stderr, "Unrecognised parameter: %s\n", arg);
			error = true;
			continue;
		}

		if (consumed)
			consumed[i] = true;

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
			*(bool *)param->ptr = true;
			continue;
		}

		if (i + 1 >= argc) {
			fprintf(stderr, "Missing value for parameter: %s\n", arg);
			error = true;
			continue;
		}

		const char *value = argv[++i];

		if (consumed)
			consumed[i] = true;

		switch (param->type) {
		case ARGPARSE_TYPE_STRING: {
			int minLength =
				param->data.s.minLength < 0 ? 0 : param->data.s.minLength;
			int maxLength =
				param->data.s.maxLength < 0 ? 1024 : param->data.s.maxLength;
			size_t length = strnlen(value, maxLength + 1);

			if (length < (size_t)minLength || length > (size_t)maxLength) {
				fprintf(stderr,
						"Invalid length for %s (expected %d-%d characters)\n",
						arg, minLength, maxLength);
				error = true;
				break;
			}

			*(const char **)param->ptr = *argv;
			break;
		}

		case ARGPARSE_TYPE_INT: {
			errno = 0;
			char *end;
			long result = strtol(value, &end, 10);

			if (errno == ERANGE || end == value || *end != '\0' ||
				result < INT_MIN || result > INT_MAX ||
				result < param->data.i.min || result > param->data.i.max) {

				fprintf(stderr, "Invalid integer for %s: %s\n", arg, value);
				error = true;
				break;
			}

			*(int *)param->ptr = (int)result;
			break;
		}

		case ARGPARSE_TYPE_FLOAT: {
			errno = 0;
			char *end;
			float result = strtof(value, &end);

			if (errno == ERANGE || end == value || *end != '\0' ||
				!isfinite(result) || result < param->data.f.min ||
				result > param->data.f.max) {

				fprintf(stderr, "Invalid float for %s: %s\n", arg, value);
				error = true;
				break;
			}

			*(float *)param->ptr = result;
			break;
		}

		case ARGPARSE_TYPE_COLOR: {
			ArgparseColor color;

			if (!parseColor(value, &color)) {
				fprintf(stderr, "Invalid color for %s: %s\n", arg, value);
				error = true;
				break;
			}

			*(ArgparseColor *)param->ptr = color;
			break;
		}

		case ARGPARSE_TYPE_CHOICE: {
			const char *const *choices = param->data.c.choices;
			int index = 0;
			bool found = false;

			if (choices) {
				for (const char *const *c = choices; *c; ++c) {
					if (strcmp(*c, value) == 0) {
						found = true;
						break;
					}
					++index;
				}
			}

			if (!found) {
				fprintf(stderr, "Invalid choice for %s: %s\n", arg, value);
				error = true;
				break;
			}

			*(int *)param->ptr = index;
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
