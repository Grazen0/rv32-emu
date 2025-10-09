#include "log.h"
#include <stdarg.h>
#include <stdio.h>

static bool verbose_mode = false;

void set_verbose(const bool verbose)
{
    verbose_mode = verbose;
}

void ver_printf(const char *const fmt, ...)
{
    if (verbose_mode) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}
