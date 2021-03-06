#ifndef _LOG_H
#define _LOG_H

#ifdef CONSOLE_ENABLE
#include "core_console.h"
#endif

#ifdef DEBUG
#ifdef CONSOLE_ENABLE
#define debugf(module, ...) \
	fprintf(stderr, "DEBUG @ " module ": " __VA_ARGS__); \
	core_console_printf("DEBUG @ " module ": " __VA_ARGS__)
#else
#define debugf(module, ...) \
	fprintf(stderr, "DEBUG @ " module ": " __VA_ARGS__)
#endif
#else
#define debugf(...) do {} while (0)
#endif

#ifdef CONSOLE_ENABLE
#define errorf(module, ...) \
	fprintf(stderr, "ERROR @ " module ": " __VA_ARGS__); \
	core_console_printf("ERROR @ " module ": " __VA_ARGS__)
#else
#define errorf(module, ...) \
	fprintf(stderr, "ERROR @ " module ": " __VA_ARGS__)
#endif

#endif
