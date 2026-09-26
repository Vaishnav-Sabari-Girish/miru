#ifndef MIRU_DEBUG_H
#define MIRU_DEBUG_H

#include <stdbool.h>

bool miru_debug_enabled(void);

/* Always log to stderr */
#define MIRU_LOG(fmt, ...) fprintf(stderr, fmt "\n" __VA_OPT__(, ) __VA_ARGS__)

/* Debug-only (MIRU_DEBUG); includes file:line */
#define MIRU_DBG(fmt, ...)                                                                       \
    do {                                                                                         \
        if (miru_debug_enabled())                                                                \
            fprintf(stderr, "[%s:%d] " fmt "\n", __FILE__, __LINE__ __VA_OPT__(, ) __VA_ARGS__); \
    } while (0)

#endif
