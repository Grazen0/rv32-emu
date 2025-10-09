#include "io.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

u8 *load_file(const char *const filename, size_t *const file_size)
{
    FILE *const file = fopen(filename, "rb");

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
