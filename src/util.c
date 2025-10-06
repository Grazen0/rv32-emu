#include "util.h"
#include "stdinc.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

size_t sz_max(const size_t a, const size_t b)
{
    return a > b ? a : b;
}

u8 *load_file(const char *const filename, size_t *const file_size)
{
    FILE *const file = fopen(filename, "r");

    if (file == nullptr)
        return nullptr;

    fseek(file, 0L, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    u8 *data = malloc(*file_size * sizeof(*data));

    if (!fread(data, sizeof(*data), *file_size, file)) {
        free(data);
        data = nullptr;
    }

    fclose(file);
    return data;
}

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
