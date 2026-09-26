#include "debug.h"
#include <stdlib.h>
#include <string.h>

bool miru_debug_enabled(void)
{
    static int cached = -1;
    if (cached == -1) {
        const char *v = getenv("MIRU_DEBUG");
        cached = (v && *v && strcmp(v, "0") != 0) ? 1 : 0;
    }
    return cached != 0;
}
