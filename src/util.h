#ifndef RV32_EMU_UTIL_H
#define RV32_EMU_UTIL_H

#include "stdinc.h"
#include <stddef.h>

[[nodiscard]] size_t sz_max(size_t a, size_t b);

/**
 * \brief Loads a file into memory.
 *
 * Returns a dynamic array that must be freed with free().
 *
 * \param filename path of the file to read
 * \param file_size out param for the size of the file read
 *
 * \return the file data in bytes, or NULL if something went wrong
 */
[[nodiscard]] u8 *load_file(const char *filename, size_t *file_size);

void set_verbose_mode(bool verbose);

void ver_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
