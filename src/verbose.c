#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "verbose.h"

bool g_htmlsplit_verbose = false; /* Extern */

/**
 * Acts like printf() if `g_htmlsplit_verbose' is set,
 * otherwise this function does nothing.
 */
void verbprintf(const char* fmt, ...)
{
    va_list arglist;

    if (g_htmlsplit_verbose) {
        va_start(arglist, fmt);
        vprintf(fmt, arglist);
        va_end(arglist);
    }
}
